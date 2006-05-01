#ifndef xine_engine
#define xine_engine

#include <xine.h>

namespace Phonon
{
namespace Xine
{

	struct XineEngine
	{
		xine_t* m_xine;
		xine_stream_t* m_stream;
		xine_audio_port_t* m_audioPort;
	};

}
}

#endif
