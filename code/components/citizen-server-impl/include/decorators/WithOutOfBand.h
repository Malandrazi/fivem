#pragma once

namespace fx
{
	namespace ServerDecorators
	{
		template<typename... TOOB>
		const fwRefContainer<fx::GameServer>& WithOutOfBand(const fwRefContainer<fx::GameServer>& server)
		{
			static std::map<ENetHost*, std::function<int(ENetHost*)>> interceptionWrappers;

			auto oobs = { TOOB()... };
			std::map<std::string, std::function<void(const fwRefContainer<fx::GameServer>& server, const AddressPair& from, const std::string_view& data)>, std::less<>> processors;

			for (const auto& oob : oobs)
			{
				processors.insert({ oob.GetName(), std::bind(&std::remove_reference_t<decltype(oob)>::Process, &oob, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) });
			}

			server->OnHostsRegistered.Connect([=]()
			{
				for (auto& host : server->hosts)
				{
					auto enHost = host.get();

					interceptionWrappers[enHost] = [=](ENetHost* host)
					{
						if (host->receivedDataLength >= 4 && *reinterpret_cast<int*>(host->receivedData) == -1)
						{
							auto begin = host->receivedData + 4;
							auto len = host->receivedDataLength - 4;

							//ProcessOOB({ host, GetPeerAddress(host->receivedAddress) }, { begin, end });
							std::string_view sv(reinterpret_cast<const char*>(begin), len);
							auto from = GetPeerAddress(host->receivedAddress);

							auto key = sv.substr(0, sv.find_first_of(" \n"));
							auto data = sv.substr(sv.find_first_of(" \n") + 1);

							auto it = processors.find(key);

							if (it != processors.end())
							{
								it->second(server, { host, from }, data);
							}

							return 1;
						}

						return 0;
					};

					// set an interceptor callback
					host->intercept = [](ENetHost* host, ENetEvent* event)
					{
						return interceptionWrappers[host](host);
					};
				}
			});

			return server;
		}
	}
}