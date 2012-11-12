/**
 * @file SDPRecord.h
 *
 */

/******************************************************************************
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

#include <map>
#include <utility>

#include <qcc/Debug.h>
#include <qcc/SDPRecord.h>
#include <qcc/Stream.h>
#include <qcc/String.h>
#include <Status.h>

#define QCC_MODULE "SDPRecord"

using namespace std;
using namespace qcc;

SDPRecord::SDPRecord(size_t numComponents) : sessionAtts(), streamList(numComponents)
{
    QCC_DbgTrace(("SDPRecord constructor"));
}

QStatus SDPRecord::Parse(Source& source)
{
    qcc::String str;
    int streamIndex = -1;
    QStatus status;

    /* Reset the stream list */
    streamList.clear();

    /* Read lines until EOF */
    while (true) {
        qcc::String att;
        qcc::String value;
        size_t colonIdx;

        str.clear();
        status = source.GetLine(str);

        if (ER_OK != status) {
            break;
        } else if ((str.size() < 2) || ('=' != str[1])) {
            continue;
        }

        switch (str[0]) {
        case 'a':
            colonIdx = str.find_first_of(':', 2);
            colonIdx = (colonIdx == qcc::String::npos) ? str.size() : colonIdx;
            att = str.substr(0, colonIdx);
            value = (str.size() > colonIdx + 1) ? str.substr(colonIdx + 1) : "";
            break;

        case 'm':
            att = str[0];
            value = str.substr(2);
            ++streamIndex;
            streamList.push_back(multimap<StreamAttribute, qcc::String>());
            break;

        default:
            att = str[0];
            value = str.substr(2);
            break;
        }

        if (-1 == streamIndex) {
            sessionAtts.insert(pair<SessionAttribute, qcc::String>(att, value));
        } else {
            streamList[streamIndex].insert(pair<StreamAttribute, qcc::String>(att, value));
        }
    }
    return (ER_NONE == status) ? ER_OK : status;
}

QStatus SDPRecord::Generate(Sink& sink) const
{
    // Output SDPRecord attributes
    // Attributes must be written out in order specified in RFC2327
    const SessionAttribute sessionAttsOrder[] = { "v", "o", "s", "i", "u", "e", "p", "c", "b", "t", "r", "z", "k", "a" };
    const StreamAttribute streamAttsOrder[] = { "m", "i", "c", "b", "k", "a" };

    // Write session attributes
    const SessionAttribute* sessionStart = &sessionAttsOrder[0];
    const SessionAttribute* sessionEnd = sessionAttsOrder + sizeof(sessionAttsOrder) / sizeof(SessionAttribute);
    while (sessionStart != sessionEnd) {
        QStatus status;
        pair<SessionAttribute, qcc::String> vals[MAX_VALUES_PER_ATTRIBUTE];
        size_t count = sizeof(vals) / sizeof(vals[0]);

        status = GetSessionAttribute(*sessionStart++, vals, vals + count, &count);
        if (ER_OK == status) {
            pair<SessionAttribute, qcc::String>* itBeg = &vals[0];
            pair<SessionAttribute, qcc::String>* itEnd = itBeg + count;
            while (itBeg != itEnd) {
                qcc::String& att = itBeg->first;
                size_t actual;
                if (att.find_first_of('=') == att.npos) {
                    sink.PushBytes((void*)att.c_str(), att.size(), actual);
                    sink.PushBytes((void*)"=", 1, actual);
                    sink.PushBytes((void*)itBeg->second.c_str(), itBeg->second.size(), actual);
                } else if (0 < itBeg->second.size()) {
                    sink.PushBytes((void*)att.c_str(), att.size(), actual);
                    sink.PushBytes((void*)":", 1, actual);
                    sink.PushBytes((void*)itBeg->second.c_str(), itBeg->second.size(), actual);
                } else {
                    sink.PushBytes((void*)att.c_str(), att.size(), actual);
                }
                sink.PushBytes((void*)"\n", 1, actual);
                ++itBeg;
            }
        }
    }

    // Write stream attributes
    for (size_t i = 0; i < streamList.size(); ++i) {
        const StreamAttribute* streamStart = &streamAttsOrder[0];
        const StreamAttribute* streamEnd = streamAttsOrder + sizeof(streamAttsOrder) / sizeof(StreamAttribute);
        while (streamStart != streamEnd) {
            QStatus status;
            pair<StreamAttribute, qcc::String> vals[MAX_VALUES_PER_ATTRIBUTE];
            size_t count = sizeof(vals) / sizeof(vals[0]);

            status = GetStreamAttribute(i, *streamStart++, vals, vals + count, &count);
            if (ER_OK == status) {
                pair<StreamAttribute, qcc::String>* itBeg = &vals[0];
                pair<StreamAttribute, qcc::String>* itEnd = itBeg + count;
                while (itBeg != itEnd) {
                    qcc::String& att = itBeg->first;
                    size_t actual;
                    if (att.find_first_of('=') == att.npos) {
                        sink.PushBytes((void*)att.c_str(), att.size(), actual);
                        sink.PushBytes((void*)"=", 1, actual);
                        sink.PushBytes((void*)itBeg->second.c_str(), itBeg->second.size(), actual);
                    } else if (0 < itBeg->second.size()) {
                        sink.PushBytes((void*)att.c_str(), att.size(), actual);
                        sink.PushBytes((void*)":", 1, actual);
                        sink.PushBytes((void*)itBeg->second.c_str(), itBeg->second.size(), actual);
                    } else {
                        sink.PushBytes((void*)att.c_str(), att.size(), actual);
                    }
                    sink.PushBytes((void*)"\n", 1, actual);
                    ++itBeg;
                }
            }
        }
    }

    return ER_OK;
}

QStatus SDPRecord::AddSessionAttribute(SessionAttribute att, qcc::String value)
{
    sessionAtts.insert(pair<SessionAttribute, qcc::String>(att, value));
    return ER_OK;
}

QStatus SDPRecord::AddStreamAttribute(int streamNum, StreamAttribute att, qcc::String value)
{
    QCC_DbgTrace(("SDPRecord::AddStreamAttribute(streamNum = %d, att = %s, value = %s)",
                  streamNum, att.c_str(), value.c_str()));
    if (streamNum < (int) streamList.size()) {
        streamList[streamNum].insert(pair<StreamAttribute, qcc::String>(att, value));
        return ER_OK;
    } else {
        return ER_FAIL;
    }
}

QStatus SDPRecord::ReplaceStreamAttribute(int streamNum, StreamAttribute att, qcc::String value)
{
    QStatus status = ER_FAIL;
    if (streamNum < (int) streamList.size()) {
        multimap<StreamAttribute, qcc::String>::iterator found = streamList[streamNum].find(att);
        if (found != streamList[streamNum].end()) {
            streamList[streamNum].erase(found);
            AddStreamAttribute(streamNum, att, value);
            status = ER_OK;
        }
    }
    return status;
}

template <typename _OutputIterator>
QStatus SDPRecord::GetSessionAttribute(SessionAttribute att, _OutputIterator begin, _OutputIterator end, size_t* count) const
{
    QStatus status = ER_OK;
    multimap<SessionAttribute, qcc::String>::const_iterator it = sessionAtts.lower_bound(att);
    multimap<SessionAttribute, qcc::String>::const_iterator itEnd = sessionAtts.end();
    *count = 0;
    while ((it != itEnd) && (0 == (*it).first.compare(0, att.size(), att))) {
        (*count)++;
        if (begin != end) {
            *begin++ = *it;
        } else {
            status = ER_FAIL;
        }
        it++;
    }
    return status;
}

template <typename _OutputIterator>
QStatus SDPRecord::GetStreamAttribute(unsigned int streamNumber, StreamAttribute att, _OutputIterator begin, _OutputIterator end, size_t* count) const
{
    QStatus status = ER_FAIL;
    if (streamNumber < streamList.size()) {
        status = ER_OK;
        multimap<StreamAttribute, qcc::String>::const_iterator it = streamList[streamNumber].lower_bound(att);
        multimap<StreamAttribute, qcc::String>::const_iterator itEnd = streamList[streamNumber].end();
        *count = 0;
        while ((it != itEnd) && (0 == (*it).first.compare(0, att.size(), att))) {
            (*count)++;
            if (begin != end) {
                *begin++ = *it;
            } else {
                status = ER_FAIL;
            }
            it++;
        }
    }
    return status;
}


