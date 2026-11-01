#include <nw4r/snd.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace snd {

/**
 * Dummy class to instantiate necessary weak functions. The real
 * McsSoundArchive (sound archive read from a host PC) is dead-stripped from
 * the channel; only the IOStream alignment getters its stream emitted remain.
 */
class McsFileStream : public ut::IOStream {
private:
    McsFileStream();
};

McsFileStream::McsFileStream() {}

} // namespace snd
} // namespace nw4r
