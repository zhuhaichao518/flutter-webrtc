#include "flutter_video_renderer.h"
#include "d3d11.h"
#include "dxgi.h"

namespace flutter_webrtc_plugin {

FlutterVideoRenderer::FlutterVideoRenderer(TextureRegistrar* registrar,
                                           BinaryMessenger* messenger)
    : registrar_(registrar) {
  /* texture_ =
      std::make_unique<flutter::TextureVariant>(flutter::PixelBufferTexture(
          [this](size_t width,
                 size_t height) -> const FlutterDesktopPixelBuffer* {
            return this->CopyPixelBuffer(width, height);
          }));*/

    texture_ =
       std::make_unique<flutter::TextureVariant>(flutter::GpuSurfaceTexture(
           //kFlutterDesktopGpuSurfaceTypeD3d11Texture2D,
          kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle,
        [this](size_t width,
                  size_t height) -> const FlutterDesktopGpuSurfaceDescriptor* {
            return this->CopyGPUBuffer(width, height);
        }));

  texture_id_ = registrar_->RegisterTexture(texture_.get());

  std::string channel_name =
      "FlutterWebRTC/Texture" + std::to_string(texture_id_);
  event_channel_ = EventChannelProxy::Create(messenger, channel_name);
}

const FlutterDesktopGpuSurfaceDescriptor* FlutterVideoRenderer::CopyGPUBuffer(
    size_t width,
    size_t height) const {
    mutex_.lock();
  if (surface_descriptor_.get() && frame_.get()) {
        surface_descriptor_->width = frame_->width();
        surface_descriptor_->height = frame_->height();

        ID3D11Texture2D* m_texture = (ID3D11Texture2D*)frame_->GetNative();
        // ����һ�� DXGI ����
        IDXGIResource* dxgiSurface;

        m_texture->QueryInterface(__uuidof(IDXGIResource),
                                  (void**)&dxgiSurface);

        // ʹ�� DXGI ���洴��������
        HANDLE sharedHandle;
        HRESULT hr = dxgiSurface->GetSharedHandle(&sharedHandle);
        if (hr < 0) {
          mutex_.unlock();
          return nullptr;
        }
        // ȷ����ʹ����ɺ��ͷ���Դ
        dxgiSurface->Release();

        //surface_descriptor_->handle = frame_->GetNative();
        surface_descriptor_->handle = sharedHandle;
        surface_descriptor_->struct_size =
            sizeof(FlutterDesktopGpuSurfaceDescriptor);
        surface_descriptor_->release_context = frame_->GetNative();

        surface_descriptor_->release_callback = [](void* release_context) {
          ID3D11Texture2D* texture2d = (ID3D11Texture2D*)release_context;
          texture2d->Release();
        };
        surface_descriptor_->format = kFlutterDesktopPixelFormatBGRA8888;
        mutex_.unlock();
        return surface_descriptor_.get();
      }
      mutex_.unlock();
      return nullptr;
}

const FlutterDesktopPixelBuffer* FlutterVideoRenderer::CopyPixelBuffer(
    size_t width,
    size_t height) const {
/* todo:(haichao):
    #ifdef HAVE_FLUTTER_D3D_TEXTURE
  texture_bridge_ =
      std::make_unique<TextureBridgeGpu>(graphics_context, webview_->surface());

  flutter_texture_ =
      std::make_unique<flutter::TextureVariant>(flutter::GpuSurfaceTexture(
          kFlutterDesktopGpuSurfaceTypeDxgiSharedHandle,
          [bridge = static_cast<TextureBridgeGpu*>(texture_bridge_.get())](
              size_t width,
              size_t height) -> const FlutterDesktopGpuSurfaceDescriptor* {
            return bridge->GetSurfaceDescriptor(width, height);
          }));
#else
  texture_bridge_ = std::make_unique<TextureBridgeFallback>(
      graphics_context, webview_->surface());

  flutter_texture_ =
      std::make_unique<flutter::TextureVariant>(flutter::PixelBufferTexture(
          [bridge = static_cast<TextureBridgeFallback*>(texture_bridge_.get())](
              size_t width, size_t height) -> const FlutterDesktopPixelBuffer* {
            return bridge->CopyPixelBuffer(width, height);
          }));
#endif
*/
  mutex_.lock();
  if (pixel_buffer_.get() && frame_.get()) {
    if (pixel_buffer_->width != frame_->width() ||
        pixel_buffer_->height != frame_->height()) {
      size_t buffer_size =
          (size_t(frame_->width()) * size_t(frame_->height())) * (32 >> 3);
      rgb_buffer_.reset(new uint8_t[buffer_size]);
      pixel_buffer_->width = frame_->width();
      pixel_buffer_->height = frame_->height();
    }

    frame_->ConvertToARGB(RTCVideoFrame::Type::kABGR, rgb_buffer_.get(), 0,
                          static_cast<int>(pixel_buffer_->width),
                          static_cast<int>(pixel_buffer_->height));

    pixel_buffer_->buffer = rgb_buffer_.get();
    mutex_.unlock();
    return pixel_buffer_.get();
  }
  mutex_.unlock();
  return nullptr;
}

void FlutterVideoRenderer::OnFrame(scoped_refptr<RTCVideoFrame> frame) {
  if (!first_frame_rendered) {
    EncodableMap params;
    params[EncodableValue("event")] = "didFirstFrameRendered";
    params[EncodableValue("id")] = EncodableValue(texture_id_);
    event_channel_->Success(EncodableValue(params));
    pixel_buffer_.reset(new FlutterDesktopPixelBuffer());
    pixel_buffer_->width = 0;
    pixel_buffer_->height = 0;
    first_frame_rendered = true;

    surface_descriptor_.reset(new FlutterDesktopGpuSurfaceDescriptor());
    surface_descriptor_->width = 0;
    surface_descriptor_->height = 0;

    hardware_accelerated = (frame->GetNative() != nullptr);
  }
  if (rotation_ != frame->rotation()) {
    EncodableMap params;
    params[EncodableValue("event")] = "didTextureChangeRotation";
    params[EncodableValue("id")] = EncodableValue(texture_id_);
    params[EncodableValue("rotation")] =
        EncodableValue((int32_t)frame->rotation());
    event_channel_->Success(EncodableValue(params));
    rotation_ = frame->rotation();
  }
  if (last_frame_size_.width != frame->width() ||
      last_frame_size_.height != frame->height()) {
    EncodableMap params;
    params[EncodableValue("event")] = "didTextureChangeVideoSize";
    params[EncodableValue("id")] = EncodableValue(texture_id_);
    params[EncodableValue("width")] = EncodableValue((int32_t)frame->width());
    params[EncodableValue("height")] = EncodableValue((int32_t)frame->height());
    event_channel_->Success(EncodableValue(params));

    last_frame_size_ = {(size_t)frame->width(), (size_t)frame->height()};
  }
  mutex_.lock();
  frame_ = frame;
  mutex_.unlock();
  registrar_->MarkTextureFrameAvailable(texture_id_);
}

void FlutterVideoRenderer::SetVideoTrack(scoped_refptr<RTCVideoTrack> track) {
  if (track_ != track) {
    if (track_)
      track_->RemoveRenderer(this);
    track_ = track;
    last_frame_size_ = {0, 0};
    first_frame_rendered = false;
    if (track_)
      track_->AddRenderer(this);
  }
}

bool FlutterVideoRenderer::CheckMediaStream(std::string mediaId) {
  if (0 == mediaId.size() || 0 == media_stream_id.size()) {
    return false;
  }
  return mediaId == media_stream_id;
}

bool FlutterVideoRenderer::CheckVideoTrack(std::string mediaId) {
  if (0 == mediaId.size() || !track_) {
    return false;
  }
  return mediaId == track_->id().std_string();
}

FlutterVideoRendererManager::FlutterVideoRendererManager(
    FlutterWebRTCBase* base)
    : base_(base) {}

void FlutterVideoRendererManager::CreateVideoRendererTexture(
    std::unique_ptr<MethodResultProxy> result) {
  std::unique_ptr<FlutterVideoRenderer> texture(
      new FlutterVideoRenderer(base_->textures_, base_->messenger_));
  int64_t texture_id = texture->texture_id();
  renderers_[texture_id] = std::move(texture);
  EncodableMap params;
  params[EncodableValue("textureId")] = EncodableValue(texture_id);
  result->Success(EncodableValue(params));
}

void FlutterVideoRendererManager::SetMediaStream(int64_t texture_id,
                                                 const std::string& stream_id,
                                                 const std::string& ownerTag) {
  scoped_refptr<RTCMediaStream> stream =
      base_->MediaStreamForId(stream_id, ownerTag);

  auto it = renderers_.find(texture_id);
  if (it != renderers_.end()) {
    FlutterVideoRenderer* renderer = it->second.get();
    if (stream.get()) {
      auto video_tracks = stream->video_tracks();
      if (video_tracks.size() > 0) {
        renderer->SetVideoTrack(video_tracks[0]);
        renderer->media_stream_id = stream_id;
      }
    } else {
      renderer->SetVideoTrack(nullptr);
    }
  }
}

void FlutterVideoRendererManager::VideoRendererDispose(
    int64_t texture_id,
    std::unique_ptr<MethodResultProxy> result) {
  auto it = renderers_.find(texture_id);
  if (it != renderers_.end()) {
    base_->textures_->UnregisterTexture(texture_id);
    renderers_.erase(it);
    result->Success();
    return;
  }
  result->Error("VideoRendererDisposeFailed",
                "VideoRendererDispose() texture not found!");
}

}  // namespace flutter_webrtc_plugin
