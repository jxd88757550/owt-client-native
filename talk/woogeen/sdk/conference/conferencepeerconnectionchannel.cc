/*
 * Intel License
 */

#include <vector>
#include "webrtc/base/logging.h"
#include "talk/woogeen/sdk/base/functionalobserver.h"
#include "talk/woogeen/sdk/conference/conferencepeerconnectionchannel.h"

namespace woogeen {

using std::string;

enum ConferencePeerConnectionChannel::SessionState : int {
  kSessionStateReady = 1,  // Indicate the channel is ready. This is the initial state.
  kSessionStateOffered,  // Indicates local client has sent an invitation and waiting for an acceptance.
  kSessionStatePending,  // Indicates local client received an invitation and waiting for user's response.
  kSessionStateMatched,  // Indicates both sides agreed to start a WebRTC session. One of them will send an offer soon.
  kSessionStateConnecting,  // Indicates both sides are trying to connect to the other side.
  kSessionStateConnected,  // Indicates PeerConnection has been established.
};

enum ConferencePeerConnectionChannel::NegotiationState : int {
  kNegotiationStateNone = 1,  // Indicates not in renegotiation.
  kNegotiationStateSent,  // Indicates a negotiation request has been sent to remote user.
  kNegotiationStateReceived,  // Indicates local side has received a negotiation request from remote user.
  kNegotiationStateAccepted,  // Indicates local side has accepted remote user's negotiation request.
};

// Const value for messages
const int kSessionIdBase = 104;  // Not sure why it should be 104, just according to JavaScript SDK.
const int kMessageSeqBase = 1;
const int kTiebreakerUpperBound = 429496723;  // ditto

// Signaling message type
const string kMessageTypeKey = "type";
const string kMessageDataKey = "data";
const string kChatInvitation = "chat-invitation";
const string kChatAccept = "chat-accepted";
const string kChatDeny = "chat-denied";
const string kChatStop = "chat-closed";
const string kChatSignal = "chat-signal";
const string kChatNegotiationNeeded = "chat-negotiation-needed";
const string kChatNegotiationAccepted = "chat-negotiation-accepted";
const string kStreamType = "stream-type";

// Stream option member key
const string kStreamOptionStateKey = "state";
const string kStreamOptionDataKey = "type";
const string kStreamOptionAudioKey = "audio";
const string kStreamOptionVideoKey = "video";
const string kStreamOptionScreenKey = "screen";
const string kStreamOptionAttributesKey = "attributes";

// Session description member key
const string kSessionDescriptionMessageTypeKey = "messageType";
const string kSessionDescriptionSdpKey = "sdp";
const string kSessionDescriptionOfferSessionIdKey = "offererSessionId";
const string kSessionDescriptionAnswerSessionIdKey = "answerSessionId";
const string kSessionDescriptionSeqKey = "seq";
const string kSessionDescriptionTiebreakerKey = "tiebreaker";

// ICE candidate member key
const string kIceCandidateSdpMidKey = "sdpMid";
const string kIceCandidateSdpMLineIndexKey = "sdpMLineIndex";
const string kIceCandidateSdpNameKey = "candidate";

ConferencePeerConnectionChannel::ConferencePeerConnectionChannel(std::shared_ptr<ConferenceSignalingChannelInterface> signaling_channel)
    :signaling_channel_(signaling_channel),
     session_id_(kSessionIdBase),
     message_seq_(kMessageSeqBase),
     session_state_(kSessionStateReady),
     negotiation_state_(kNegotiationStateNone),
     callback_thread_(new PeerConnectionThread) {
  callback_thread_->Start();
  InitializePeerConnection();
  CHECK(signaling_channel_);
}

void ConferencePeerConnectionChannel::ChangeSessionState(SessionState state) {
  LOG(LS_INFO) << "PeerConnectionChannel change session state : " << state;
  session_state_ = state;
}

void ConferencePeerConnectionChannel::ChangeNegotiationState(NegotiationState state) {
  LOG(LS_INFO) << "PeerConnectionChannel change negotiation state : " << state;
  negotiation_state_ = state;
}

void ConferencePeerConnectionChannel::AddObserver(ConferencePeerConnectionChannelObserver* observer) {
  observers_.push_back(observer);
}

void ConferencePeerConnectionChannel::RemoveObserver(ConferencePeerConnectionChannelObserver *observer) {
  observers_.erase(std::remove(observers_.begin(), observers_.end(), observer), observers_.end());
}

void ConferencePeerConnectionChannel::CreateOffer() {
  LOG(LS_INFO) << "Create offer.";
  scoped_refptr<FunctionalCreateSessionDescriptionObserver> observer=FunctionalCreateSessionDescriptionObserver::Create(std::bind(&ConferencePeerConnectionChannel::OnCreateSessionDescriptionSuccess, this, std::placeholders::_1), std::bind(&ConferencePeerConnectionChannel::OnCreateSessionDescriptionFailure, this, std::placeholders::_1));
  rtc::TypedMessageData<scoped_refptr<FunctionalCreateSessionDescriptionObserver>>* data = new rtc::TypedMessageData<scoped_refptr<FunctionalCreateSessionDescriptionObserver>>(observer);
  LOG(LS_INFO) << "Post create offer";
  pc_thread_->Post(this, kMessageTypeCreateOffer, data);
}

void ConferencePeerConnectionChannel::CreateAnswer() {
  LOG(LS_INFO) << "Create answer.";
  scoped_refptr<FunctionalCreateSessionDescriptionObserver> observer=FunctionalCreateSessionDescriptionObserver::Create(std::bind(&ConferencePeerConnectionChannel::OnCreateSessionDescriptionSuccess, this, std::placeholders::_1), std::bind(&ConferencePeerConnectionChannel::OnCreateSessionDescriptionFailure, this, std::placeholders::_1));
  rtc::TypedMessageData<scoped_refptr<FunctionalCreateSessionDescriptionObserver>>* message_observer = new rtc::TypedMessageData<scoped_refptr<FunctionalCreateSessionDescriptionObserver>>(observer);
  LOG(LS_INFO) << "Post create answer";
  pc_thread_->Post(this, kMessageTypeCreateAnswer, message_observer);
}

void ConferencePeerConnectionChannel::SendSignalingMessage(const Json::Value& data, std::function<void()> success, std::function<void(std::unique_ptr<ConferenceException>)> failure) {
}

void ConferencePeerConnectionChannel::OnSignalingChange(PeerConnectionInterface::SignalingState new_state) {
  LOG(LS_INFO) << "Signaling state changed: " << new_state;
}

void ConferencePeerConnectionChannel::OnAddStream(MediaStreamInterface* stream) {
}

void ConferencePeerConnectionChannel::OnRemoveStream(MediaStreamInterface* stream) {
}

void ConferencePeerConnectionChannel::OnDataChannel(webrtc::DataChannelInterface* data_channel) {
  // TODO:
}

void ConferencePeerConnectionChannel::OnRenegotiationNeeded() {
}

void ConferencePeerConnectionChannel::OnIceConnectionChange(PeerConnectionInterface::IceConnectionState new_state) {
  LOG(LS_INFO) << "Ice connection state changed: " << new_state;
}

void ConferencePeerConnectionChannel::OnIceGatheringChange(PeerConnectionInterface::IceGatheringState new_state) {
  LOG(LS_INFO) << "Ice gathering state changed: " << new_state;
  if(new_state==PeerConnectionInterface::kIceGatheringComplete){
    auto local_desc = LocalDescription();
    Json::Value sdp_info;
    std::string sdp_string;
    if(!local_desc->ToString(&sdp_string)){
      LOG(LS_ERROR)<<"Error parsing local description.";
    }
    sdp_info[kSessionDescriptionMessageTypeKey]="OFFER";
    sdp_info[kSessionDescriptionSdpKey]=sdp_string;
    sdp_info[kSessionDescriptionOfferSessionIdKey]=session_id_;
    sdp_info[kSessionDescriptionSeqKey]=message_seq_;
    sdp_info[kSessionDescriptionTiebreakerKey]= 89884208;
    Json::Value options;
    options[kStreamOptionStateKey]=local_desc->type();
    // TODO(jianjun): set correct options.
    options[kStreamOptionDataKey]=true;
    options[kStreamOptionAudioKey]=true;
    options[kStreamOptionVideoKey]=true;
    std::string sdp_message=JsonValueToString(sdp_info);
    signaling_channel_->Publish(options, sdp_message, nullptr, nullptr);
  }
}

void ConferencePeerConnectionChannel::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
  LOG(LS_INFO) << "On ice candidate";
}

void ConferencePeerConnectionChannel::OnCreateSessionDescriptionSuccess(webrtc::SessionDescriptionInterface* desc) {
  LOG(LS_INFO) << "Create sdp success.";
  scoped_refptr<FunctionalSetSessionDescriptionObserver> observer=FunctionalSetSessionDescriptionObserver::Create(
      std::bind(&ConferencePeerConnectionChannel::OnSetLocalSessionDescriptionSuccess, this),
      std::bind(&ConferencePeerConnectionChannel::OnSetLocalSessionDescriptionFailure, this, std::placeholders::_1));
  SetSessionDescriptionMessage* msg = new SetSessionDescriptionMessage(observer.get(), desc);
  LOG(LS_INFO) << "Post set local desc";
  pc_thread_->Post(this, kMessageTypeSetLocalDescription, msg);
}

void ConferencePeerConnectionChannel::OnCreateSessionDescriptionFailure(const std::string& error) {
  LOG(LS_INFO) << "Create sdp failed.";
}

void ConferencePeerConnectionChannel::OnSetLocalSessionDescriptionSuccess() {
  LOG(LS_INFO) << "Set local sdp success.";
}

void ConferencePeerConnectionChannel::OnSetLocalSessionDescriptionFailure(const std::string& error) {
  LOG(LS_INFO) << "Set local sdp failed.";
}

void ConferencePeerConnectionChannel::OnSetRemoteSessionDescriptionSuccess() {
  PeerConnectionChannel::OnSetRemoteSessionDescriptionSuccess();
}

void ConferencePeerConnectionChannel::OnSetRemoteSessionDescriptionFailure(const std::string& error) {
  LOG(LS_INFO) << "Set remote sdp failed.";
}

bool ConferencePeerConnectionChannel::CheckNullPointer(uintptr_t pointer, std::function<void(std::unique_ptr<ConferenceException>)>on_failure) {
  if(pointer)
    return true;
  if(on_failure!=nullptr) {
    std::unique_ptr<ConferenceException> e(new ConferenceException(ConferenceException::kUnkown, "Nullptr is not allowed."));
    on_failure(std::move(e));
  }
  return false;
}

void ConferencePeerConnectionChannel::Publish(std::shared_ptr<LocalStream> stream, std::function<void()> on_success, std::function<void(std::unique_ptr<ConferenceException>)> on_failure) {
  LOG(LS_INFO) << "Publish a local stream.";
  if(!CheckNullPointer((uintptr_t)stream.get(), on_failure)){
    LOG(LS_INFO) << "Local stream cannot be nullptr.";
    return;
  }
  rtc::TypedMessageData<MediaStreamInterface*>* param = new rtc::TypedMessageData<MediaStreamInterface*>(stream->MediaStream());
  pc_thread_->Post(this, kMessageTypeAddStream, param);
  CreateOffer();
}

void ConferencePeerConnectionChannel::Unpublish(std::shared_ptr<LocalStream> stream, std::function<void()> on_success, std::function<void(std::unique_ptr<ConferenceException>)> on_failure) {
  if(!CheckNullPointer((uintptr_t)stream.get(), on_failure)){
    LOG(LS_INFO) << "Local stream cannot be nullptr.";
    return;
  }
}

void ConferencePeerConnectionChannel::Stop(std::function<void()> on_success, std::function<void(std::unique_ptr<ConferenceException>)> on_failure) {
  LOG(LS_INFO) << "Stop session.";
}

int ConferencePeerConnectionChannel::RandomInt(int lower_bound, int upper_bound) {
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_real_distribution<int> dist(1, kTiebreakerUpperBound);
  return dist(mt);
}

}