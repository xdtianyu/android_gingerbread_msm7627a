/**
 * @file SDPRecord.h
 *
 */

/******************************************************************************
 * Copyright 2010 - 2011, Qualcomm Innovation Center, Inc.
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
 *
 ******************************************************************************/

#ifndef _QCC_SDPRECORD_H
#define _QCC_SDPRECORD_H

#include <qcc/platform.h>

#include <map>
#include <vector>
#include <qcc/String.h>
#include <qcc/Stream.h>

#include <Status.h>

namespace qcc {

/**
 * An SDPRecord is a binary representation of a Session Description Protocol (SDP) record.
 * The contents of an SDP record are described in RFC2327.
 *
 * SDPRecord can be used to parse and generate RFC2327 compliant byte streams.
 *
 * ICE uses SDP records to communicate media stream information including ICE candidates to potential remote peers.
 * These SDP records are exhanged via a secure OOB mechanism. In the case of rendezvous server based discovery,
 * these SDP records are exchanged using HTTP over TLS/SSL.
 *
 * @sa http://www.ietf.org/rfc/rfc2327.txt
 *
 */
struct SDPRecord {
  public:

    /** Session attribute types */
    typedef qcc::String SessionAttribute;

    /** Stream attribute types */
    typedef qcc::String StreamAttribute;

    /**
     * Constructor an SDPRecord with a number of components
     *
     * @param numComponents    Number of components in the SDP record. Defaults to 1.
     *                         When parsing a text SDPRecord, this parameter is ignored.
     */
    SDPRecord(size_t numComponents = 1);

    /**
     * Populate the SDPRecord with an RFC2327 formatted input stream.
     * @param sin   Input stream containing ASCII formatted SDP record.
     * @return ER_OK if successful.
     */
    QStatus Parse(Source& sin);

    /**
     * Generate an RFC2327 compliant representation of this SDPRecord.
     * @param sout   Output stream used to write ASCII formatted SDP record.
     * @return  ER_OK if successful.
     */
    QStatus Generate(Sink& sout) const;

    /**
     * Add a session-level attribute.
     *
     * @param att    Type of attribute to add.
     * @param value  Value for attribute.
     * @return  ICE_OK if successful.
     */
    QStatus AddSessionAttribute(SessionAttribute att, qcc::String value);

    /**
     * Add a media stream level attribute.
     *
     * @param stream   Index of media stream to receive new attribute.
     * @param att      Type of attribute to add.
     * @param value    Value for attribute.
     * @return  ICE_OK if successful.
     */
    QStatus AddStreamAttribute(int stream, StreamAttribute att, qcc::String value);

    /**
     * Replace a media stream level attribute.
     *
     * @param stream   Index of media stream to receive replaced attribute.
     * @param att      Type of attribute to replace.
     * @param value    Value for attribute.
     * @return  ICE_OK if successful.
     */
    QStatus ReplaceStreamAttribute(int stream, StreamAttribute att, qcc::String value);

    /**
     * Get a (possibly) multi-valued session-level attribute.
     *
     * @param att        Type of attribute to retrieve.
     * @param begin      Begin iterator for attribute values. (May be 0 if only interested in count.)
     * @param end        End iterator for attrbute values. (May be 0 if only interested in count.)
     * @param numVals    (OUT) Number of attribute values that exist for this attribute.
     * @return  ICE_OK if successful.
     */
    template <typename _OutputIterator>
    QStatus GetSessionAttribute(SessionAttribute att,
                                _OutputIterator begin,
                                _OutputIterator end,
                                size_t* numVals) const;


    /**
     * Get a (possibly) multi-valued media stream level attribute.
     *
     * @param streamNumber    Zero-based index of media stream to receive new attribute.
     * @param att             Type of attribute to retrieve.
     * @param begin           Begin iterator for attribute values
     * @param end             End iterator for attribute values
     * @param numVals         (OUT) Number of attribute values that exist for this attribute.
     * @return  ICE_OK if successful.
     */
    template <typename _OutputIterator>
    QStatus GetStreamAttribute(unsigned int streamNumber,
                               StreamAttribute att,
                               _OutputIterator begin,
                               _OutputIterator end,
                               size_t* numVals) const;

    /**
     * Get the number of media streams.
     *
     * @return  The number of media streams.
     */
    uint16_t GetStreamCount(void) const { return streamList.size(); }

  private:

    /** Session level attribute container */
    std::multimap<SessionAttribute, qcc::String> sessionAtts;

    /** Stream containers */
    std::vector<std::multimap<StreamAttribute, qcc::String> > streamList;

};

const int MAX_VALUES_PER_ATTRIBUTE = 50; // per RFC2327 plus arbitrary number of ICE candidates

}

#endif
