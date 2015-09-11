// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <cstdlib>
#include <cstddef>
#include <iostream>

#include <SoapyRPCSocket.hpp>
#include <SoapySocketSession.hpp>

int main(int argc, char *argv[])
{
    std::cout << "######################################################" << std::endl;
    std::cout << "## Soapy Server -- Use any Soapy SDR remotely" << std::endl;
    std::cout << "######################################################" << std::endl;
    std::cout << std::endl;

    SoapySocketSession sess;

    SoapyRPCSocket s;
    s.bind(argv[1]);

    return EXIT_SUCCESS;
}
