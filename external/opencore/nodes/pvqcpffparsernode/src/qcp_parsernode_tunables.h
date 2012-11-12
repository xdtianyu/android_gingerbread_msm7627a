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
#ifndef PVMF_QCPFFPARSER_TUNABLES_H_INCLUDED
#define PVMF_QCPFFPARSER_TUNABLES_H_INCLUDED

//Default NODE COMMAND vector reserve size
#define PVMF_QCPFFPARSER_NODE_COMMAND_VECTOR_RESERVE 10

//Starting value for command IDs
#define PVMF_QCPFFPARSER_NODE_COMMAND_ID_START 6000

#define PVQCPFF_MEDIADATA_POOLNUM 6
#define PVQCPFF_MEDIADATA_CHUNKSIZE 128

// Temporary until actual max track data size if used.
#define MAXTRACKDATASIZE	1024
#define MAX_PORTS			2
#define NUM_QCP_FRAMES		1

#endif /*PVMF_QCPFFPARSER_TUNABLES_H_INCLUDED*/
