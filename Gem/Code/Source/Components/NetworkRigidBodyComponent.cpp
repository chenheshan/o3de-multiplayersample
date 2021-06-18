/*
 * All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
 * its licensors.
 *
 * For complete copyright and license terms please see the LICENSE at the root of this
 * distribution (the "License"). All use of this software is governed by the License,
 * or, if provided, by the license below or the license accompanying this file. Do not
 * remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 */

#include <Source/Components/NetworkRigidBodyComponent.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Physics/RigidBodyBus.h>
#include <AzFramework/Physics/SimulatedBodies/RigidBody.h>

namespace MultiplayerSample
{
    AZ_CVAR_EXTERNED(float, bg_RewindPositionTolerance);
    AZ_CVAR_EXTERNED(float, bg_RewindOrientationTolerance);

    void NetworkRigidBodyComponent::NetworkRigidBodyComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<NetworkRigidBodyComponent, NetworkRigidBodyComponentBase>()->Version(1);
        }
        NetworkRigidBodyComponentBase::Reflect(context);
    }

    NetworkRigidBodyComponent::NetworkRigidBodyComponent()
        : m_syncRewindHandler([this](){ OnSyncRewind(); })
        , m_transformChangedHandler([this]([[maybe_unused]] const AZ::Transform& localTm, const AZ::Transform& worldTm){ OnTransformUpdate(worldTm); })
    {

    }

    void NetworkRigidBodyComponent::OnInit()
    {
    }

    void NetworkRigidBodyComponent::OnActivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
        GetNetBindComponent()->AddEntitySyncRewindEventHandler(m_syncRewindHandler);
        GetEntity()->FindComponent<AzFramework::TransformComponent>()->BindTransformChangedEventHandler(m_transformChangedHandler);

        m_physicsRigidBodyComponent =
            Physics::RigidBodyRequestBus::FindFirstHandler(GetEntity()->GetId());
        AZ_Assert(m_physicsRigidBodyComponent, "PhysX Rigid Body Component is required on entity %s", GetEntity()->GetName().c_str());

        if (!HasController())
        {
            m_physicsRigidBodyComponent->SetKinematic(true);
        }
    }

    void NetworkRigidBodyComponent::OnDeactivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    void NetworkRigidBodyComponent::OnTransformUpdate(const AZ::Transform& worldTm)
    {
        m_transform = worldTm;

        if (!HasController())
        {
            m_physicsRigidBodyComponent->SetKinematicTarget(worldTm);
        }
    }

    void NetworkRigidBodyComponent::OnSyncRewind()
    {
        uint32_t frameId = static_cast<uint32_t>(Multiplayer::GetNetworkTime()->GetHostFrameId());

        AzPhysics::RigidBody* rigidBody = m_physicsRigidBodyComponent->GetRigidBody();
        rigidBody->SetFrameId(frameId);

        const AZ::Transform& rewoundTransform = m_transform.Get();
        const AZ::Transform& physicsTransform = rigidBody->GetTransform();

        // Don't call SetLocalPose unless the transforms are actually different
        const AZ::Vector3 positionDelta = physicsTransform.GetTranslation() - rewoundTransform.GetTranslation();
        const AZ::Quaternion orientationDelta = physicsTransform.GetRotation() - rewoundTransform.GetRotation();

        if ((positionDelta.GetLengthSq() >= bg_RewindPositionTolerance) ||
            (orientationDelta.GetLengthSq() >= bg_RewindOrientationTolerance))
        {
            rigidBody->SetTransform(rewoundTransform);
        }
    }

} // namespace MultiplayerSample