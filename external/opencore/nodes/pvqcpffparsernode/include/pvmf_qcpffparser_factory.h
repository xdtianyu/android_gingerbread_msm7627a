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
#ifndef PVMF_QCPFFPARSER_FACTORY_H_INCLUDED
#define PVMF_QCPFFPARSER_FACTORY_H_INCLUDED

#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif

// Forward declaration
class PVMFNodeInterface;

#define KPVMFQCPFFParserNodeUuid PVUuid(0x041878c0,0x1934,0x11de,0xbd,0x6f,0x00,0x02,0xa5,0xd5,0xc5,0x1b)


/**
 * PVMFQCPFFParserNodeFactory Class
 *
 * PVMFQCPFFParserNodeFactory class is a singleton class which instantiates and provides
 * access to PVMF QCP(QCELP/EVRC) audio decoder node. It returns a PVMFNodeInterface
 * reference, the interface class of the PVMFQCPFFParserNode.
 *
 * The client is expected to contain and maintain a pointer to the instance created
 * while the node is active.
 */
class PVMFQCPFFParserNodeFactory
{
    public:
        /**
         * Creates an instance of a PVMFQCPFFParserNode. If the creation fails, this function will leave.
         *
         * @param aPriority The active object priority for the node. Default is standard priority if not specified
         * @returns A pointer to an instance of PVMFQCPFFParserNode as PVMFNodeInterface reference or leaves if instantiation fails
         **/
        OSCL_IMPORT_REF static PVMFNodeInterface* CreatePVMFQCPFFParserNode(int32 aPriority = OsclActiveObject::EPriorityNominal);

        /**
         * Deletes an instance of PVMFQCPFFParserNode
         * and reclaims all allocated resources.  An instance can be deleted only in
         * the idle state. An attempt to delete in any other state will fail and return false.
         *
         * @param aNode The PVMFQCPFFParserNode instance to be deleted
         * @returns A status code indicating success or failure of deletion
         **/
        OSCL_IMPORT_REF static bool DeletePVMFQCPFFParserNode(PVMFNodeInterface* aNode);
};



#endif // PVMF_QCPFFPARSER_FACTORY_H_INCLUDED

