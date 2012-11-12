/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef CND_IPROUTE2_H
#define CND_IPROUTE2_H

/**----------------------------------------------------------------------------
  @file cnd_iproute2.h

        cnd_iproute2 is an interface to make the necessary calls to iproute2
        in order to set up and take down routing tables. Defines APIs so that
        a routing table associated with a network interface can be added or
        deleted. Also allows the user to change the default routing table when
        a given source address is not already associated with a network
        interface.
-----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include <sys/types.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Class Definitions
 * -------------------------------------------------------------------------*/

class cnd_iproute2
{
  public:
    /**
    * @brief Returns an instance of the Cnd_iproute2 class.
    *
    * The user of this class will call this function to get an
    * instance of the class. All other public functions will be
    * called on this instance
    *
    * @param   None
    * @see     None
    * @return  An instance of the Cnd_iproute2 class is returned.
    */
    static cnd_iproute2* getInstance
    (
      void
    );

    /**
    * @brief Creates a custom route in the main table using iproute2
    *
    * The user of this function passes in the name of the network
    * interface that matches the name already defined in the Android
    * system. This radio will handle any packets that are sent to
    * the inputted destination address of a host. Optionally, a user
    * can pass the gateway address of the device.
    *
    * @param destinationAddress  The destination address of the
    *                            custom entry in the main table that
    *                            will be added
    * @param deviceName          The name of the device that will
    *                            handle packets to the host
    * @param gatewayAddress      The gateway address of the device
    *                            (optional)
    * @return                    True if function is successful.
    *                            False otherwise.
    */
    bool addCustomEntryInMainTable
    (
      uint8_t *destinationAddress,
      uint8_t *deviceName,
      uint8_t *gatewayAddress
    );

    /**
    * @brief Create a routing table for a network interface using
    *        iproute2
    *
    * The user of this function passes in the name of the network
    * interface that matches the name already defined in the Android
    * system. The user also needs to locate the source prefix, and,
    * optionally, the gateway address assocated with that radio. If
    * a table is added when no another tables exist, it will
    * automatically become the default table.
    *
    * @param deviceName       The name of the device whose table
    *                         will be added
    * @param sourcePrefix     The source network prefix or address
    *                         that will be routed to the device
    *                         (Such as 37.214.21/24 or 10.156.45.1)
    * @param gatewayAddress   The gateway address of the device
    *                         (optional)
    * @return                 True if function is successful. False
    *                         otherwise.
    */
    bool addRoutingTable
    (
      uint8_t *deviceName,
      uint8_t *sourcePrefix,
      uint8_t *gatewayAddress
    );

    /**
    * @brief Deletes the custom route to the inputted destination
    *        address in the main table using iproute2
    *
    * The custom route being deleted should have been added via
    * addCustomEntryToMainTable()
    *
    * @param destinationAddress  The destination address of the
    *                            custom entry in the main table that
    *                            will be removed
    * @return                    True if function is successful.
    *                            False otherwise.
    */
    bool deleteCustomEntryInMainTable
    (
      uint8_t *destinationAddress
    );

    /**
    * @brief Deletes all custom routes in the main table that route
    *        through a specific interface name
    *
    * The custom routes being deleted should have been added via
    * addCustomEntryToMainTable()
    *
    * @param deviceName       The name of the device whose custom
    *                         entries in the main table will be
    *                         removed
    * @return                 True if function is successful. False
    *                         otherwise.
    */
    bool deleteDeviceCustomEntriesInMainTable
    (
      uint8_t *deviceName
    );

    /**
    * @brief Deletes a default entry from the main table.
    *
    * @param deviceName       The name of the device whose default
    *                         entry in the main table will be
    *                         deleted
    * @return                 True if function is successful. False
    *                         otherwise.
    */
    bool deleteDefaultEntryInMainTable
    (
      uint8_t *deviceName
    );

    /**
    * @brief Deletes a routing table from the system along with the
    *        rule corresponding to that table.
    *
    * The table being deleted should have been added via
    * addRoutingTable()
    *
    * @param deviceName       The name of the device whose table will be
    *                         deleted
    * @return                 True if function is successful. False
    *                         otherwise.
    */
    bool deleteRoutingTable
    (
      uint8_t *deviceName
    );

    /**
    * @brief Change the default routing table that is associated
    *        with any source addresses not bound to another table.
    *
    * The user of this function passes in the name of the network
    * interface that matches the name already defined in the Android
    * system. That device will become the new default. If this radio
    * is already the default, this function simply returns true.
    *
    * @param deviceName       The name of the device whose table
    *                         will be added
    * @return                 True if function is successful. False
    *                         otherwise.
    */
    bool replaceDefaultEntryInMainTable
    (
      uint8_t *deviceName,
      uint8_t *gatewayAddress
    );

    /**
    * Displays the contents of all routing tables for debugging
    * purposes.
    *
    * @return                 True if function is successful. False
    *                         otherwise.
    */
    bool showAllRoutingTables
    (
      void
    );

    /**
    * Displays the contents of the routing table associated with
    * the inputted device name.
    *
    * @param deviceName       The name of the device to be displayed
    * @return                 True if function is successful. False
    *                         otherwise.
    */
    bool showRoutingTable
    (
      uint8_t *deviceName
    );

    /**
    * Displays the rules associated with all tables for debugging
    * purposes.
    *
    * @return                 True if function is successful. False
    *                         otherwise.
    */
    bool showRules
    (
      void
    );

  private:
    /* constructor */
    cnd_iproute2(){};
    /* destructor */
    ~cnd_iproute2(){};

    static cnd_iproute2* instancePtr;
};


#endif /* CND_IPROUTE2_H*/
