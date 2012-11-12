/**
 * @file SslSocket.cc
 *
 * SSL stream-based socket implementation for Linux
 */

/******************************************************************************
 *
 *
 * Copyright 2009-2011, Qualcomm Innovation Center, Inc.
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

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <signal.h>

#include <qcc/String.h>
#include <qcc/Config.h>
#include <qcc/Debug.h>
#include <qcc/IPAddress.h>
#include <qcc/Mutex.h>
#include <qcc/SslSocket.h>
#include <qcc/Stream.h>

#include <Status.h>

#define QCC_MODULE  "SSL"

using namespace std;
using namespace qcc;

static SSL_CTX* sslCtx = NULL;
static Mutex ctxMutex;

SslSocket::SslSocket()
    : bio(NULL),
    sourceEvent(&Event::neverSet),
    sinkEvent(&Event::neverSet)
{
    /* Initialize the global SSL context is this is the first SSL socket */
    if (!sslCtx) {
        ctxMutex.Lock();
        /* Check sslCtx again now that we have the mutex */
        if (!sslCtx) {
            SSL_library_init();
            SSL_load_error_strings();
            ERR_load_BIO_strings();
            OpenSSL_add_all_algorithms();
            sslCtx = SSL_CTX_new(SSLv23_client_method());
            if (sslCtx) {
                Config* cfg = Config::GetConfig();
                qcc::String trustStore = cfg->GetValue("SSL_TRUST_STORE");
                if (!SSL_CTX_load_verify_locations(sslCtx, trustStore.c_str(), NULL)) {
                    QCC_LogError(ER_SSL_INIT, ("Cannot initialize SSL trust store \"%s\"", trustStore.c_str()));
                    SSL_CTX_free(sslCtx);
                    sslCtx = NULL;
                }
            }
            if (!sslCtx) {
                QCC_LogError(ER_SSL_INIT, ("OpenSSL error is \"%s\"", ERR_reason_error_string(ERR_get_error())));
            }

            /* SSL generates SIGPIPE which we don't want */
            signal(SIGPIPE, SIG_IGN);
        }
        ctxMutex.Unlock();
    }
}

SslSocket::~SslSocket() {
    Close();
}

QStatus SslSocket::Connect(const char* hostName, uint16_t port)
{
    QStatus status = ER_OK;

    /* Sanity check */
    if (!sslCtx) {
        QCC_LogError(ER_SSL_INIT, ("SSL failed to initialize"));
        return ER_SSL_INIT;
    }

    /* Create the descriptor for this SSL socket */
    SSL* ssl;
    bio = BIO_new_ssl_connect(sslCtx);

    if (bio) {
        /* Set SSL modes */
        BIO_get_ssl(bio, &ssl);
        SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);

        /* Set destination host name and port */
        int intPort = (int) port;
        BIO_set_conn_hostname(bio, hostName);
        BIO_set_conn_int_port(bio, &intPort);

        /* Connect to destination */
        if (0 < BIO_do_connect(bio)) {
            /* Verify the certificate */
            if (SSL_get_verify_result(ssl) != X509_V_OK) {
                status = ER_SSL_VERIFY;
            }
        } else {
            status = ER_SSL_CONNECT;
        }
    } else {
        status = ER_SSL_CONNECT;
    }

    /* Set the events */
    if (ER_OK == status) {
        int fd = BIO_get_fd(bio, 0);
        sourceEvent = new Event(fd, Event::IO_READ, false);
        sinkEvent = new Event(fd, Event::IO_WRITE, false);
    }

    /* Cleanup on error */
    if (bio && (ER_OK != status)) {
        BIO_free_all(bio);
        bio = NULL;
    }

    /* Log any errors */
    if (ER_OK != status) {
        QCC_LogError(status, ("Failed to connect SSL socket"));
    }

    return status;
}

void SslSocket::Close()
{
    if (bio) {
        BIO_free_all(bio);
        bio = NULL;
    }

    if (sourceEvent != &Event::neverSet) {
        delete sourceEvent;
        sourceEvent = &Event::neverSet;
    }
    if (sinkEvent != &Event::neverSet) {
        delete sinkEvent;
        sinkEvent = &Event::neverSet;
    }
}

QStatus SslSocket::PullBytes(void* buf, size_t reqBytes, size_t& actualBytes, uint32_t timeout)
{
    QStatus status;
    int r;

    if (!bio) {
        return ER_FAIL;
    }

    r = BIO_read(bio, buf, reqBytes);

    if (0 == r) {
        actualBytes = r;
        status = ER_NONE;
    } else if (0 > r) {
        status = ER_FAIL;
        QCC_LogError(status, ("BIO_read failed with error=%d", ERR_get_error()));
    } else {
        actualBytes = r;
        status = ER_OK;
    }
    return status;
}

QStatus SslSocket::PushBytes(const void* buf, size_t numBytes, size_t& numSent)
{
    QStatus status;
    int s;

    if (!bio) {
        return ER_FAIL;
    }

    s = BIO_write(bio, buf, numBytes);

    if (0 < s) {
        status = ER_OK;
        numSent = s;
    } else {
        status = ER_FAIL;
        QCC_LogError(status, ("BIO_write failed with error=%d", ERR_get_error()));
    }

    return status;
}

