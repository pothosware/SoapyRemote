// Copyright (c) 2015-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SoapyServer.hpp"
#include "SoapyRemoteDefs.hpp"
#include "SoapyURLUtils.hpp"
#include "SoapyInfoUtils.hpp"
#include "SoapyRPCSocket.hpp"
#include "SoapySSDPEndpoint.hpp"
#include "SoapyMDNSEndpoint.hpp"
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <getopt.h>
#include <csignal>

/***********************************************************************
 * Print help message
 **********************************************************************/
static int printHelp(void)
{
    std::cout << "Usage SoapySDRServer [options]" << std::endl;
    std::cout << "  Options summary:" << std::endl;
    std::cout << "    --help \t\t\t\t Print this help message" << std::endl;
    std::cout << "    --bind \t\t\t\t Bind and serve forever" << std::endl;
    std::cout << std::endl;
    return EXIT_SUCCESS;
}

/***********************************************************************
 * Signal handler for Ctrl + C
 **********************************************************************/
static sig_atomic_t serverDone = false;
void sigIntHandler(const int)
{
    std::cout << "Caught Ctrl+C, shutting down the server..." << std::endl;
    serverDone = true;
}

/***********************************************************************
 * Launch the server
 **********************************************************************/
static int runServer(void)
{
    SoapySocketSession sess;
    const bool isIPv6Supported = not SoapyRPCSocket(SoapyURL("tcp", "::", "0").toString()).null();
    const auto defaultBindNode = isIPv6Supported?"::":"0.0.0.0";
    const int ipVerServices = isIPv6Supported?SOAPY_REMOTE_IPVER_UNSPEC:SOAPY_REMOTE_IPVER_INET;

    //extract url from user input or generate automatically
    const bool optargHasURL = (optarg != NULL and not std::string(optarg).empty());
    auto url = (optargHasURL)? SoapyURL(optarg) : SoapyURL("tcp", defaultBindNode, "");

    //default url parameters when not specified
    if (url.getScheme().empty()) url.setScheme("tcp");
    if (url.getService().empty()) url.setService(SOAPY_REMOTE_DEFAULT_SERVICE);

    //this UUID identifies the server process
    const auto serverUUID = SoapyInfo::generateUUID1();
    std::cout << "Server version: " << SoapyInfo::getServerVersion() << std::endl;
    std::cout << "Server UUID: " << serverUUID << std::endl;

    std::cout << "Launching the server... " << url.toString() << std::endl;
    SoapyRPCSocket s;
    if (s.bind(url.toString()) != 0)
    {
        std::cerr << "Server socket bind FAIL: " << s.lastErrorMsg() << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "Server bound to " << s.getsockname() << std::endl;
    s.listen(SOAPY_REMOTE_LISTEN_BACKLOG);
    auto serverListener = new SoapyServerListener(s, serverUUID);

    std::cout << "Launching discovery server... " << std::endl;
    auto ssdpEndpoint = new SoapySSDPEndpoint();
    ssdpEndpoint->registerService(serverUUID, url.getService(), ipVerServices);

    std::cout << "Connecting to DNS-SD daemon... " << std::endl;
    auto dnssdPublish = new SoapyMDNSEndpoint();
    dnssdPublish->printInfo();
    dnssdPublish->registerService(serverUUID, url.getService(), ipVerServices);

    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    signal(SIGINT, sigIntHandler);
    bool exitFailure = false;
    while (not serverDone and not exitFailure)
    {
        serverListener->handleOnce();
        if (not s.status())
        {
            std::cerr << "Server socket failure: " << s.lastErrorMsg() << std::endl;
            exitFailure = true;
        }
        if (not dnssdPublish->status())
        {
            std::cerr << "DNS-SD daemon disconnected..." << std::endl;
            exitFailure = true;
        }
    }
    if (exitFailure) std::cerr << "Exiting prematurely..." << std::endl;

    delete ssdpEndpoint;
    delete dnssdPublish;

    std::cout << "Shutdown client handler threads" << std::endl;
    delete serverListener;
    s.close();

    std::cout << "Cleanup complete, exiting" << std::endl;
    return exitFailure?EXIT_FAILURE:EXIT_SUCCESS;
}

/***********************************************************************
 * Parse and dispatch options
 **********************************************************************/
int main(int argc, char *argv[])
{
    std::cout << "######################################################" << std::endl;
    std::cout << "## Soapy Server -- Use any Soapy SDR remotely" << std::endl;
    std::cout << "######################################################" << std::endl;
    std::cout << std::endl;

    /*******************************************************************
     * parse command line options
     ******************************************************************/
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"bind", optional_argument, 0, 'b'},
        {0, 0, 0,  0}
    };
    int long_index = 0;
    int option = 0;
    while ((option = getopt_long_only(argc, argv, "", long_options, &long_index)) != -1)
    {
        switch (option)
        {
        case 'h': return printHelp();
        case 'b': return runServer();
        }
    }

    //unknown or unspecified options, do help...
    return printHelp();
}
