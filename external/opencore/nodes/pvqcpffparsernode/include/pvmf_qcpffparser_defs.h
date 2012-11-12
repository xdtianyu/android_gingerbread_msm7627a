/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#ifndef PVMF_QCPFFPARSER_DEFS_H_INCLUDED
#define PVMF_QCPFFPARSER_DEFS_H_INCLUDED

///////////////////////////////////////////////
// Port tags
///////////////////////////////////////////////

/** Enumerated list of port tags supported by the node,
** for the port requests.
*/
typedef enum
{
    PVMF_QCPFFPARSER_NODE_PORT_TYPE_SOURCE
} PVMFQCPFFParserOutPortType;

// Capability mime strings
#define PVMF_QCPFFPARSER_PORT_OUTPUT_FORMATS "x-pvmf/port/formattype"
#define PVMF_QCPFFPARSER_PORT_OUTPUT_FORMATS_VALTYPE "x-pvmf/port/formattype;valtype=int32"

#define QCP_MIN_DATA_SIZE_FOR_RECOGNITION  194
#define QCP_DATA_OBJECT_PARSING_OVERHEAD   512
#define QCP_HEADER_SIZE 194

#define PVMF_QCP_PARSER_NODE_MAX_AUDIO_DATA_MEM_POOL_SIZE   1024*1024
#define PVMF_QCP_PARSER_NODE_DATA_MEM_POOL_GROWTH_LIMIT     1
#define PVMF_QCP_PARSER_NODE_MAX_NUM_OUTSTANDING_MEDIA_MSGS 4
#define PVMF_QCP_PARSER_NODE_MEDIA_MSG_SIZE                 128
#define PVMF_QCP_PARSER_NODE_TS_DELTA_DURING_REPOS_IN_MS    10

#endif

