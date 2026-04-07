#pragma once

#include "../../transport/TransportTraits.h"

#include <memory>
#include <utility>

#if defined(ARDUINO)

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#if __has_include(<WiFiUdp.h>)
#include <WiFiUdp.h>
#endif

#include <string>

#endif

namespace httpadv::v1::platform::arduino {

#if defined(ARDUINO)

using httpadv::v1::transport::AvailableBytes;
using httpadv::v1::transport::AvailableResult;
using httpadv::v1::transport::ErrorResult;
using httpadv::v1::transport::ExhaustedResult;
using httpadv::v1::transport::IClient;
using httpadv::v1::transport::IPeer;
using httpadv::v1::transport::IServer;
using httpadv::v1::transport::TemporarilyUnavailableResult;

namespace detail {

inline AvailableResult mapLegacyAvailable(int availableValue, bool connected) {
	if (availableValue > 0) {
		return AvailableBytes(static_cast<std::size_t>(availableValue));
	}

	if (availableValue == 0) {
		return connected ? TemporarilyUnavailableResult() : ExhaustedResult();
	}

	return ErrorResult(availableValue);
}

inline std::string ipAddressToStdString(const IPAddress &address) {
	return std::string(address.toString().c_str());
}

inline bool parseIpAddress(std::string_view addressText, IPAddress &address) {
	const std::string addressString(addressText);
	return address.fromString(addressString.c_str()) != 0;
}

} // namespace detail

class ArduinoWiFiClientAdapter : public IClient {
public:
	explicit ArduinoWiFiClientAdapter(WiFiClient client)
			: client_(std::move(client)) {}

	void stop() override { client_.stop(); }

	bool connected() override { return client_.connected(); }

	std::string_view remoteAddress() const override {
		auto &client = const_cast<WiFiClient &>(client_);
		remoteAddress_ = detail::ipAddressToStdString(client.remoteIP());
		return remoteAddress_;
	}

	std::uint16_t remotePort() override { return client_.remotePort(); }

	std::string_view localAddress() const override {
		auto &client = const_cast<WiFiClient &>(client_);
		localAddress_ = detail::ipAddressToStdString(client.localIP());
		return localAddress_;
	}

	std::uint16_t localPort() override { return client_.localPort(); }

	void setTimeout(std::uint32_t timeoutMs) override {
		timeoutMs_ = timeoutMs;
		client_.setTimeout(timeoutMs_);
	}

	std::uint32_t getTimeout() const override { return timeoutMs_; }

	AvailableResult available() override {
		return detail::mapLegacyAvailable(client_.available(), client_.connected());
	}

	std::size_t read(httpadv::v1::util::span<std::uint8_t> buffer) override {
		return buffer.empty() ? 0 : client_.read(buffer.data(), buffer.size());
	}

	std::size_t peek(httpadv::v1::util::span<std::uint8_t> buffer) override {
		if (buffer.empty()) {
			return 0;
		}

		const int value = client_.peek();
		if (value < 0) {
			return 0;
		}

		buffer[0] = static_cast<std::uint8_t>(value);
		return 1;
	}

	std::size_t write(httpadv::v1::util::span<const std::uint8_t> buffer) override {
		return buffer.empty() ? 0 : client_.write(buffer.data(), buffer.size());
	}

	void flush() override { client_.flush(); }

private:
	mutable std::string remoteAddress_{};
	mutable std::string localAddress_{};
	std::uint32_t timeoutMs_ = 0;
	WiFiClient client_{};
};

class ArduinoWiFiServerAdapter : public IServer {
public:
	explicit ArduinoWiFiServerAdapter(std::uint16_t port)
			: server_(port), port_(port) {}

	std::unique_ptr<IClient> accept() override {
		WiFiClient client = server_.accept();
		if (!client) {
			return nullptr;
		}

		return std::make_unique<ArduinoWiFiClientAdapter>(std::move(client));
	}

	void begin() override { server_.begin(); }

	std::uint16_t port() const override { return port_; }

	std::string_view localAddress() const override {
		localAddress_ = detail::ipAddressToStdString(WiFi.localIP());
		return localAddress_;
	}

	void end() override {}

private:
	mutable std::string localAddress_{};
	WiFiServer server_;
	std::uint16_t port_ = 0;
};

class ArduinoWiFiPeerAdapter : public IPeer {
public:
	bool begin(std::uint16_t port) override {
		active_ = udp_.begin(port) != 0;
		return active_;
	}

	bool beginMulticast(std::string_view multicastAddress,
											std::uint16_t port) override {
		IPAddress address;
		if (!detail::parseIpAddress(multicastAddress, address)) {
			return false;
		}

#if defined(ARDUINO_ARCH_ESP8266)
		active_ = udp_.beginMulticast(WiFi.localIP(), address, port) != 0;
#else
		active_ = udp_.beginMulticast(address, port) != 0;
#endif
		return active_;
	}

	void stop() override {
		udp_.stop();
		active_ = false;
	}

	bool beginPacket(std::string_view address, std::uint16_t port) override {
		IPAddress remoteAddress;
		if (!detail::parseIpAddress(address, remoteAddress)) {
			return false;
		}

		return udp_.beginPacket(remoteAddress, port) != 0;
	}

	bool endPacket() override { return udp_.endPacket() != 0; }

	std::size_t write(httpadv::v1::util::span<const std::uint8_t> buffer) override {
		return buffer.empty() ? 0 : udp_.write(buffer.data(), buffer.size());
	}

	AvailableResult parsePacket() override {
		const int packetSize = udp_.parsePacket();
		if (packetSize > 0) {
			return AvailableBytes(static_cast<std::size_t>(packetSize));
		}

		return active_ ? TemporarilyUnavailableResult() : ExhaustedResult();
	}

	AvailableResult available() override {
		return detail::mapLegacyAvailable(udp_.available(), active_);
	}

	std::size_t read(httpadv::v1::util::span<std::uint8_t> buffer) override {
		return buffer.empty() ? 0 : udp_.read(buffer.data(), buffer.size());
	}

	std::size_t peek(httpadv::v1::util::span<std::uint8_t> buffer) override {
		if (buffer.empty()) {
			return 0;
		}

		const int value = udp_.peek();
		if (value < 0) {
			return 0;
		}

		buffer[0] = static_cast<std::uint8_t>(value);
		return 1;
	}

	void flush() override { udp_.flush(); }

	std::string_view remoteAddress() const override {
		auto &udp = const_cast<WiFiUDP &>(udp_);
		remoteAddress_ = detail::ipAddressToStdString(udp.remoteIP());
		return remoteAddress_;
	}

	std::uint16_t remotePort() const override {
		auto &udp = const_cast<WiFiUDP &>(udp_);
		return udp.remotePort();
	}

private:
	mutable std::string remoteAddress_{};
	bool active_ = false;
	WiFiUDP udp_{};
};

#endif

class ArduinoWiFiTransportFactory {
public:
#if defined(ARDUINO)
	static std::unique_ptr<IServer> createServer(std::uint16_t port) {
		return std::make_unique<ArduinoWiFiServerAdapter>(port);
	}

	static std::unique_ptr<IClient> createClient(std::string_view address,
																							 std::uint16_t port) {
		const std::string host(address);
		WiFiClient client;
		if (!client.connect(host.c_str(), port)) {
			return nullptr;
		}

		return std::make_unique<ArduinoWiFiClientAdapter>(std::move(client));
	}

	static std::unique_ptr<IPeer> createPeer() {
		return std::make_unique<ArduinoWiFiPeerAdapter>();
	}

	static std::string getHostByName(std::string_view hostName) {
		if (hostName.empty()) {
			return {};
		}

		const std::string host(hostName);
		IPAddress address;
		if (!WiFi.hostByName(host.c_str(), address)) {
			return {};
		}

		return detail::ipAddressToStdString(address);
	}
#else
	static std::unique_ptr<transport::IServer> createServer(std::uint16_t) {
		return nullptr;
	}

	static std::unique_ptr<transport::IClient> createClient(std::string_view,
																													std::uint16_t) {
		return nullptr;
	}

	static std::unique_ptr<transport::IPeer> createPeer() {
		return nullptr;
	}

	static std::string getHostByName(std::string_view) {
		return {};
	}
#endif
};

} // namespace httpadv::v1::platform::arduino

