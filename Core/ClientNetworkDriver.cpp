#include "stdafx.h"
#include "ClientNetworkDriver.h"
#include "TCPSocket.h"
#include "Commands.h"
#include "Shapes.h"
#include "IClientDriver.h"
#include "Image.h"
#include "Packet.h"
#include "IO_Runner.h"
#include "turbojpeg.h"

namespace SL {
	namespace Remote_Access_Library {
		namespace Network {

			class ClientNetworkDriverImpl : public IBaseNetworkDriver {
				IClientDriver* _IClientDriver;
				std::shared_ptr<Network::TCPSocket> _Socket;
				std::unique_ptr<IO_Runner> _IO_Runner;
				std::string _dst_host, _dst_port;
			public:
				ClientNetworkDriverImpl(IClientDriver* r, const char * dst_host, const char * dst_port) : _IClientDriver(r),  _dst_host(dst_host), _dst_port(dst_port) {

				}
				void Start() {
					Stop();
					_IO_Runner = std::move(std::make_unique<IO_Runner>());
					_Socket = std::make_shared<TCPSocket>(this, _IO_Runner->get_io_service());
					_Socket->connect(_dst_host.c_str(), _dst_port.c_str());
				}
				void Stop() {
					if (_Socket) _Socket->close();
					if (_IO_Runner) _IO_Runner->Stop();
				}
				virtual ~ClientNetworkDriverImpl() {
					Stop();
				}
				virtual void OnConnect(const std::shared_ptr<ISocket>& socket) override { _IClientDriver->OnConnect(socket); }
				virtual void OnClose(const std::shared_ptr<ISocket>& socket) override { _IClientDriver->OnClose(socket); }

				virtual void OnReceive(const std::shared_ptr<ISocket>& socket, std::shared_ptr<Packet>& p) override {

					switch (p->header()->Packet_Type) {
					case static_cast<unsigned int>(Commands::PACKET_TYPES::IMAGEDIF) :
						ImageDif(socket, p);
						break;
					default:
						_IClientDriver->OnReceive(socket, p);//pass up the chain
						break;
					}

				}

				void ImageDif(const std::shared_ptr<ISocket>& socket, std::shared_ptr<Packet>& p) {
					auto imgrect = (Utilities::Rect*)p->data();
					auto compfree = [](void* handle) {tjDestroy(handle); };
					auto _jpegDecompressor(std::unique_ptr<void, decltype(compfree)>(tjInitDecompress(), compfree));

					int jpegSubsamp = 0;
					auto outwidth = 0;
					auto outheight = 0;

					auto src = (unsigned char*)(p->data() + sizeof(Utilities::Rect));

					if (tjDecompressHeader2(_jpegDecompressor.get(), src, p->header()->PayloadLen - sizeof(Utilities::Rect), &outwidth, &outheight, &jpegSubsamp) == -1) {
						std::cout << "Err msg " << tjGetErrorStr() << std::endl;
					}
					auto img = Utilities::Image::CreateImage(outheight, outwidth);

					if (tjDecompress2(_jpegDecompressor.get(), src, p->header()->PayloadLen - sizeof(Utilities::Rect), (unsigned char*)img->data(), outwidth, 0, outheight, TJPF_BGRX, TJFLAG_FASTDCT | TJFLAG_NOREALLOC) == -1) {
						std::cout << "Err msg " << tjGetErrorStr() << std::endl;
					}


					_IClientDriver->OnReceive_Image(socket, imgrect, img);

				}
			};


		}
	}
}


SL::Remote_Access_Library::Network::ClientNetworkDriver::ClientNetworkDriver(IClientDriver * r, const char * dst_host, const char * dst_port) : _ClientNetworkDriverImpl(std::make_unique<ClientNetworkDriverImpl>(r, dst_host, dst_port))
{

}

SL::Remote_Access_Library::Network::ClientNetworkDriver::~ClientNetworkDriver()
{
	Stop();
}

void SL::Remote_Access_Library::Network::ClientNetworkDriver::Start()
{
	_ClientNetworkDriverImpl->Start();
}

void SL::Remote_Access_Library::Network::ClientNetworkDriver::Stop()
{
	_ClientNetworkDriverImpl->Stop();
}