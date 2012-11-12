/**
 * @file
 *
 * This file tests SRP against the RFC 5054 test vector
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/

#include <qcc/platform.h>

#include <qcc/Crypto.h>
#include <qcc/Debug.h>
#include <qcc/KeyBlob.h>
#include <qcc/String.h>
#include <qcc/StringUtil.h>
#include <qcc/Util.h>
#include <qcc/Debug.h>

#include <alljoyn/version.h>

#include <Status.h>

#define QCC_MODULE "CRYPTO"

using namespace qcc;
using namespace std;

int main(int argc, char** argv)
{
    String toClient;
    String toServer;
    String verifier;
    QStatus status = ER_OK;
    KeyBlob serverPMS;
    KeyBlob clientPMS;
    String user = "someuser";
    String pwd = "a-secret-password";

    printf("AllJoyn Library version: %s\n", ajn::GetVersion());
    printf("AllJoyn Library build info: %s\n", ajn::GetBuildInfo());

    /* Test vector as defined in RFC 5246 built in to Crypto_SRP class */
    {
        Crypto_SRP srp;
        status = srp.TestVector();
        if (status != ER_OK) {
            printf("Test vector failed\n");
        }
    }
    printf("#################################\n");
    /*
     * Basic API test.
     */
    {
        Crypto_SRP client;
        Crypto_SRP server;

        status = server.ServerInit(user, pwd, toClient);
        if (status != ER_OK) {
            QCC_LogError(status, ("SRP ServerInit failed"));
            goto TestFail;
        }
        status = client.ClientInit(toClient, toServer);
        if (status != ER_OK) {
            QCC_LogError(status, ("SRP ClientInit failed"));
            goto TestFail;
        }
        status = server.ServerFinish(toServer);
        if (status != ER_OK) {
            QCC_LogError(status, ("SRP ServerFinish failed"));
            goto TestFail;
        }
        status = client.ClientFinish(user, pwd);
        if (status != ER_OK) {
            QCC_LogError(status, ("SRP ClientFinish failed"));
            goto TestFail;
        }
        /*
         * Check premaster secrets match
         */
        server.GetPremasterSecret(serverPMS);
        client.GetPremasterSecret(clientPMS);
        if (clientPMS.GetSize() != serverPMS.GetSize()) {
            printf("Premaster secrets have different sizes\n");
            goto TestFail;
        }
        if (memcmp(serverPMS.GetData(), clientPMS.GetData(), serverPMS.GetSize()) != 0) {
            printf("Premaster secrets don't match\n");
            goto TestFail;
        }
        printf("Premaster secret = %s\n", BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str());
        verifier = server.ServerGetVerifier();
    }
    printf("#################################\n");
    /*
     * Test using the verifier from the previous test.
     */
    {
        Crypto_SRP client;
        Crypto_SRP server;

        status = server.ServerInit(verifier, toClient);
        if (status != ER_OK) {
            QCC_LogError(status, ("SRP ServerInit failed"));
            goto TestFail;
        }
        status = client.ClientInit(toClient, toServer);
        if (status != ER_OK) {
            QCC_LogError(status, ("SRP ClientInit failed"));
            goto TestFail;
        }
        status = server.ServerFinish(toServer);
        if (status != ER_OK) {
            QCC_LogError(status, ("SRP ServerFinish failed"));
            goto TestFail;
        }
        status = client.ClientFinish(user, pwd);
        if (status != ER_OK) {
            QCC_LogError(status, ("SRP ClientFinish failed"));
            goto TestFail;
        }
        /*
         * Check premaster secrets match
         */
        server.GetPremasterSecret(serverPMS);
        client.GetPremasterSecret(clientPMS);
        if (clientPMS.GetSize() != serverPMS.GetSize()) {
            printf("Premaster secrets have different sizes\n");
            goto TestFail;
        }
        if (memcmp(serverPMS.GetData(), clientPMS.GetData(), serverPMS.GetSize()) != 0) {
            printf("Premaster secrets don't match\n");
            goto TestFail;
        }
        printf("Premaster secret = %s\n", BytesToHexString(serverPMS.GetData(), serverPMS.GetSize()).c_str());

        qcc::String serverRand = RandHexString(64);
        qcc::String clientRand = RandHexString(64);
        uint8_t masterSecret[48];

        status = Crypto_PseudorandomFunction(serverPMS, "foobar", serverRand + clientRand, masterSecret, sizeof(masterSecret));
        if (status != ER_OK) {
            QCC_LogError(status, ("Crypto_PseudoRandomFunction failed"));
            goto TestFail;
        }
        printf("Master secret = %s\n", BytesToHexString(masterSecret, sizeof(masterSecret)).c_str());

    }
    printf("#################################\n");

    printf("Passed\n");
    return 0;

TestFail:

    printf("Failed\n");
    return -1;
}
