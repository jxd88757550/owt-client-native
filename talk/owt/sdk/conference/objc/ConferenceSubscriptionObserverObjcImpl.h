// Copyright (C) <2018> Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

#ifndef OWT_CONFERENCE_OBJC_CONFERENCESUBSCRIPTIONOBJCIMPL_H_
#define OWT_CONFERENCE_OBJC_CONFERENCESUBSCRIPTIONOBJCIMPL_H_
#include "talk/owt/sdk/include/cpp/owt/base/subscription.h"
#import "talk/owt/sdk/include/objc/OWT/OWTConferenceSubscription.h"
namespace owt {
namespace conference {
class ConferenceSubscriptionObserverObjcImpl
    : public owt::base::SubscriptionObserver {
 public:
  ConferenceSubscriptionObserverObjcImpl(
      OWTConferenceSubscription* subscription,
      id<OWTConferenceSubscriptionDelegate> delegate)
      : subscription_(subscription), delegate_(delegate) {}
 protected:
  /// Triggered when publication is ended.
  virtual void OnEnded() override;
  /// Triggered when audio and/or video is muted.
  virtual void OnMute(owt::base::TrackKind track_kind) override;
  /// Triggered when audio and/or video is unmuted.
  virtual void OnUnmute(owt::base::TrackKind track_kind) override;
 private:
  OWTConferenceSubscription* subscription_;
  id<OWTConferenceSubscriptionDelegate> delegate_;
};
}  // namespace conference
}  // namespace owt
#endif
