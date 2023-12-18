#include "flutter_data_channel.h"

#include <vector>

namespace flutter_webrtc_plugin {

FlutterRTCDataChannelObserver::FlutterRTCDataChannelObserver(
    scoped_refptr<RTCDataChannel> data_channel,
    BinaryMessenger* messenger,
    const std::string& channelName)
    : event_channel_(EventChannelProxy::Create(messenger, channelName)),
      data_channel_(data_channel) {
  data_channel_->RegisterObserver(this);
}

FlutterRTCDataChannelObserver::~FlutterRTCDataChannelObserver() {}

void FlutterDataChannel::CreateDataChannel(
    const std::string& peerConnectionId,
    const std::string& label,
    const EncodableMap& dataChannelDict,
    RTCPeerConnection* pc,
    std::unique_ptr<MethodResultProxy> result) {
  RTCDataChannelInit init;
  init.id = GetValue<int>(dataChannelDict.find(EncodableValue("id"))->second);
  init.ordered =
      GetValue<bool>(dataChannelDict.find(EncodableValue("ordered"))->second);

  if (dataChannelDict.find(EncodableValue("maxRetransmits")) !=
      dataChannelDict.end()) {
    init.maxRetransmits = GetValue<int>(
        dataChannelDict.find(EncodableValue("maxRetransmits"))->second);
  }

  std::string protocol = "sctp";

  if (dataChannelDict.find(EncodableValue("protocol")) ==
      dataChannelDict.end()) {
    protocol = GetValue<std::string>(
        dataChannelDict.find(EncodableValue("protocol"))->second);
  }

  init.protocol = protocol;

  init.negotiated = GetValue<bool>(
      dataChannelDict.find(EncodableValue("negotiated"))->second);

  scoped_refptr<RTCDataChannel> data_channel =
      pc->CreateDataChannel(label.c_str(), &init);

  std::string uuid = base_->GenerateUUID();
  std::string event_channel =
      "FlutterWebRTC/dataChannelEvent" + peerConnectionId + uuid;

  std::unique_ptr<FlutterRTCDataChannelObserver> observer(
      new FlutterRTCDataChannelObserver(data_channel, base_->messenger_,
                                        event_channel));

  base_->lock();
  base_->data_channel_observers_[uuid] = std::move(observer);
  base_->unlock();

  EncodableMap params;
  params[EncodableValue("id")] = EncodableValue(init.id);
  params[EncodableValue("label")] =
      EncodableValue(data_channel->label().std_string());
  params[EncodableValue("flutterId")] = EncodableValue(uuid);
  result->Success(EncodableValue(params));
}

static std::map<RTCDataChannel*, HWINEVENTHOOK> hookedchannels;

HANDLE GetCursorHandle(HCURSOR hCursor) {
  static HCURSOR hArrow = LoadCursor(NULL, IDC_ARROW);
  if (hCursor == hArrow)
    return IDC_ARROW;

  static HCURSOR hIBeam = LoadCursor(NULL, IDC_IBEAM);
  if (hCursor == hIBeam)
    return IDC_IBEAM;

  static HCURSOR hWait = LoadCursor(NULL, IDC_WAIT);
  if (hCursor == hWait)
    return IDC_WAIT;

  static HCURSOR hCross = LoadCursor(NULL, IDC_CROSS);
  if (hCursor == hCross)
    return IDC_CROSS;

  static HCURSOR hUpArrow = LoadCursor(NULL, IDC_UPARROW);
  if (hCursor == hUpArrow)
    return IDC_UPARROW;

  // IDC_SIZE and IDC_ICON are out of date
  static HCURSOR hSize = LoadCursor(NULL, IDC_SIZE);
  if (hCursor == hSize)
    return IDC_SIZE;

  static HCURSOR hIcon = LoadCursor(NULL, IDC_ICON);
  if (hCursor == hIcon)
    return IDC_ICON;

  static HCURSOR hSizeNWSE = LoadCursor(NULL, IDC_SIZENWSE);
  if (hCursor == hSizeNWSE)
    return IDC_SIZENWSE;

  static HCURSOR hSizeNESW = LoadCursor(NULL, IDC_SIZENESW);
  if (hCursor == hSizeNESW)
    return IDC_SIZENESW;

  static HCURSOR hSizeWE = LoadCursor(NULL, IDC_SIZEWE);
  if (hCursor == hSizeWE)
    return IDC_SIZEWE;

  static HCURSOR hSizeNS = LoadCursor(NULL, IDC_SIZENS);
  if (hCursor == hSizeNS)
    return IDC_SIZENS;

  static HCURSOR hSizeAll = LoadCursor(NULL, IDC_SIZEALL);
  if (hCursor == hSizeAll)
    return IDC_SIZEALL;

  static HCURSOR hNo = LoadCursor(NULL, IDC_NO);
  if (hCursor == hNo)
    return IDC_NO;

#if (WINVER >= 0x0500)
  static HCURSOR hHand = LoadCursor(NULL, IDC_HAND);
  if (hCursor == hHand)
    return IDC_HAND;
#endif

  static HCURSOR hAppStarting = LoadCursor(NULL, IDC_APPSTARTING);
  if (hCursor == hAppStarting)
    return IDC_APPSTARTING;

#if (WINVER >= 0x0400)
  static HCURSOR hHelp = LoadCursor(NULL, IDC_HELP);
  if (hCursor == hHelp)
    return IDC_HELP;
#endif

#if (WINVER >= 0x0606)
  static HCURSOR hPin = LoadCursor(NULL, IDC_PIN);
  if (hCursor == hPin)
    return IDC_PIN;

  static HCURSOR hPerson = LoadCursor(NULL, IDC_PERSON);
  if (hCursor == hPerson)
    return IDC_PERSON;
#endif

  return NULL;
}

void CursorChangedEventProc(HWINEVENTHOOK hook2,
                           DWORD event,
                           HWND hwnd,
                           LONG idObject,
                           LONG idChild,
                           DWORD idEventThread,
                           DWORD time) {
  if (hwnd == nullptr && idObject == OBJID_CURSOR && idChild == CHILDID_SELF) {
    std::string str;
    switch (event) {
      case EVENT_OBJECT_HIDE:
        str = "csrhide";
        for (auto hookedchannel : hookedchannels) {
          hookedchannel.first->Send(reinterpret_cast<const uint8_t*>(str.c_str()),
                              static_cast<uint32_t>(str.length()), false);
        }
        break;
      case EVENT_OBJECT_SHOW:
        str = "csrshow";
        for (auto hookedchannel : hookedchannels) {
          hookedchannel.first->Send(
              reinterpret_cast<const uint8_t*>(str.c_str()),
                               static_cast<uint32_t>(str.length()), false);
        }
        break;
      case EVENT_OBJECT_NAMECHANGE:
        str = "csrchaged";
        CURSORINFO ci = {sizeof(ci)};
        GetCursorInfo(&ci);
        HANDLE h = GetCursorHandle(ci.hCursor);
        if (h != NULL) {
          str = "csrid: " + std::to_string(reinterpret_cast<intptr_t>(h));
          for (auto hookedchannel : hookedchannels) {
            hookedchannel.first->Send(
                reinterpret_cast<const uint8_t*>(str.c_str()),
                                 static_cast<uint32_t>(str.length()), false);
          }
        } /* too hard! let me do it tomorrow.
          else {
          ICONINFO iconInfo;
          BITMAP bmpCursor;
          DWORD* bits;
          if (GetIconInfo(ci.hCursor, &iconInfo)) {
            GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmpCursor);

            int width = bmpCursor.bmWidth;
            int height = bmpCursor.bmHeight;

            bits = new DWORD[width * height];

            int buffersize = sizeof() sizeof(DWORD)* 2 uint8_t* buffer = ()
            // ... (获取位图数据的代码)
          }
          str = "cuscsr:";
        }*/

        break;
    }
  }
}

void FlutterDataChannel::DataChannelSend(
    RTCDataChannel* data_channel,
    const std::string& type,
    const EncodableValue& data,
    std::unique_ptr<MethodResultProxy> result) {
  bool is_binary = type == "binary";
  if (is_binary && TypeIs<std::vector<uint8_t>>(data)) {
    std::vector<uint8_t> buffer = GetValue<std::vector<uint8_t>>(data);
    data_channel->Send(buffer.data(), static_cast<uint32_t>(buffer.size()),
                       true);
  } else {
    std::string str = GetValue<std::string>(data);
    // It is ugly to implement here, but actually we should bind a cursorchange to a data channel.
    // So we implement it here for convenience.
    if (str == "csrhook") {

      auto hook = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_NAMECHANGE, nullptr,
                              CursorChangedEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);

      hookedchannels[data_channel] = hook;
    }else{
      data_channel->Send(reinterpret_cast<const uint8_t*>(str.c_str()),
                       static_cast<uint32_t>(str.length()), false);
    }
  }
  result->Success();
}

void FlutterDataChannel::DataChannelClose(
    RTCDataChannel* data_channel,
    const std::string& data_channel_uuid,
    std::unique_ptr<MethodResultProxy> result) {
  if (hookedchannels.find(data_channel) != hookedchannels.end()) {
    UnhookWinEvent(hookedchannels[data_channel]);
  }
  data_channel->Close();
  auto it = base_->data_channel_observers_.find(data_channel_uuid);
  if (it != base_->data_channel_observers_.end())
    base_->data_channel_observers_.erase(it);
  result->Success();
}

RTCDataChannel* FlutterDataChannel::DataChannelForId(const std::string& uuid) {
  auto it = base_->data_channel_observers_.find(uuid);

  if (it != base_->data_channel_observers_.end()) {
    FlutterRTCDataChannelObserver* observer = it->second.get();
    scoped_refptr<RTCDataChannel> data_channel = observer->data_channel();
    return data_channel.get();
  }
  return nullptr;
}

static const char* DataStateString(RTCDataChannelState state) {
  switch (state) {
    case RTCDataChannelConnecting:
      return "connecting";
    case RTCDataChannelOpen:
      return "open";
    case RTCDataChannelClosing:
      return "closing";
    case RTCDataChannelClosed:
      return "closed";
  }
  return "";
}

void FlutterRTCDataChannelObserver::OnStateChange(RTCDataChannelState state) {
  EncodableMap params;
  params[EncodableValue("event")] = EncodableValue("dataChannelStateChanged");
  params[EncodableValue("id")] = EncodableValue(data_channel_->id());
  params[EncodableValue("state")] = EncodableValue(DataStateString(state));
  auto data = EncodableValue(params);
  event_channel_->Success(data);
}

void FlutterRTCDataChannelObserver::OnMessage(const char* buffer,
                                              int length,
                                              bool binary) {
  EncodableMap params;
  params[EncodableValue("event")] = EncodableValue("dataChannelReceiveMessage");

  params[EncodableValue("id")] = EncodableValue(data_channel_->id());
  params[EncodableValue("type")] = EncodableValue(binary ? "binary" : "text");
  std::string str(buffer, length);
  params[EncodableValue("data")] =
      binary ? EncodableValue(std::vector<uint8_t>(str.begin(), str.end()))
             : EncodableValue(str);

  auto data = EncodableValue(params);
  event_channel_->Success(data);
}
}  // namespace flutter_webrtc_plugin
