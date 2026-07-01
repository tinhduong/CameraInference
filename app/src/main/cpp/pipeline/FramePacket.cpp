#include "FramePacket.h"

FramePacket::FramePacket(uint64_t fid, uint64_t ts, int w, int h, int rot, int fmt)
    : frameId(fid),
      timestampNs(ts),
      width(w),
      height(h),
      rotationDegrees(rot),
      format(fmt),
      startTime(std::chrono::steady_clock::now()) {}

FramePacket::~FramePacket() {
    // Vector buffers are automatically freed upon destruction.
}
