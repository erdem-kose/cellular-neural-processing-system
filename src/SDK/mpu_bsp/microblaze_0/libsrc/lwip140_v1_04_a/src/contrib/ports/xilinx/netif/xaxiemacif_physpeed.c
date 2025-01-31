/*
 * Copyright (c) 2007-2008, Advanced Micro Devices, Inc.
 *               All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Advanced Micro Devices, Inc. nor the names
 *      of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Some portions copyright (c) 2010-2013 Xilinx, Inc.  All rights reserved.
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

#include "netif/xaxiemacif.h"
#include "lwipopts.h"

/* Advertisement control register. */
#define ADVERTISE_10HALF	0x0020  /* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL	0x0020  /* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL	0x0040  /* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF	0x0040  /* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF	0x0080  /* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE	0x0080  /* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL	0x0100  /* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM	0x0100  /* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4	0x0200  /* Try for 100mbps 4k packets  */


#define ADVERTISE_100_AND_10	(ADVERTISE_10FULL | ADVERTISE_100FULL | \
				ADVERTISE_10HALF | ADVERTISE_100HALF)
#define ADVERTISE_100		(ADVERTISE_100FULL | ADVERTISE_100HALF)
#define ADVERTISE_10		(ADVERTISE_10FULL | ADVERTISE_10HALF)

#define ADVERTISE_1000		0x0300


#define IEEE_CONTROL_REG_OFFSET			0
#define IEEE_STATUS_REG_OFFSET			1
#define IEEE_AUTONEGO_ADVERTISE_REG		4
#define IEEE_PARTNER_ABILITIES_1_REG_OFFSET	5
#define IEEE_PARTNER_ABILITIES_2_REG_OFFSET	8
#define IEEE_PARTNER_ABILITIES_3_REG_OFFSET	10
#define IEEE_1000_ADVERTISE_REG_OFFSET		9
#define IEEE_COPPER_SPECIFIC_CONTROL_REG	16
#define IEEE_SPECIFIC_STATUS_REG		17
#define IEEE_COPPER_SPECIFIC_STATUS_REG_2	19
#define IEEE_CONTROL_REG_MAC			21
#define IEEE_PAGE_ADDRESS_REGISTER		22

#define IEEE_CTRL_1GBPS_LINKSPEED_MASK		0x2040
#define IEEE_CTRL_LINKSPEED_MASK		0x0040
#define IEEE_CTRL_LINKSPEED_1000M		0x0040
#define IEEE_CTRL_LINKSPEED_100M		0x2000
#define IEEE_CTRL_LINKSPEED_10M			0x0000
#define IEEE_CTRL_RESET_MASK			0x8000
#define IEEE_CTRL_AUTONEGOTIATE_ENABLE		0x1000
#define IEEE_STAT_AUTONEGOTIATE_CAPABLE		0x0008
#define IEEE_STAT_AUTONEGOTIATE_COMPLETE	0x0020
#define IEEE_STAT_AUTONEGOTIATE_RESTART		0x0200
#define IEEE_STAT_1GBPS_EXTENSIONS		0x0100
#define IEEE_AN1_ABILITY_MASK			0x1FE0
#define IEEE_AN3_ABILITY_MASK_1GBPS		0x0C00
#define IEEE_AN1_ABILITY_MASK_100MBPS		0x0380
#define IEEE_AN1_ABILITY_MASK_10MBPS		0x0060
#define IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK	0x0030

#define IEEE_ASYMMETRIC_PAUSE_MASK		0x0800
#define IEEE_PAUSE_MASK				0x0400
#define IEEE_AUTONEG_ERROR_MASK			0x8000

#define PHY_DETECT_REG  	1
#define PHY_DETECT_MASK 	0x1808
#define PHY_R0_ISOLATE  	0x0400
#define PHY_MODEL_NUM_MASK	0x3F0

/* Marvel PHY flags */
#define MARVEL_PHY_IDENTIFIER 		0x141
#define MARVEL_PHY_88E1111_MODEL	0xC0
#define MARVEL_PHY_88E1116R_MODEL	0x240

#define PHY_88E1111_RGMII_RX_CLOCK_DELAYED_MASK	0x0080

/* Loop counters to check for reset done
 */
#define RESET_TIMEOUT	500

static int detect_phy(XAxiEthernet *xaxiemacp)
{
	u16 phy_reg;
	u32 phy_addr;

	for (phy_addr = 31; phy_addr > 0; phy_addr--) {
		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, PHY_DETECT_REG,
								&phy_reg);

		if ((phy_reg != 0xFFFF) &&
			((phy_reg & PHY_DETECT_MASK) == PHY_DETECT_MASK)) {
			/* Found a valid PHY address */
			LWIP_DEBUGF(NETIF_DEBUG, ("XAxiEthernet detect_phy: PHY detected at address %d.\r\n", phy_addr));
			LWIP_DEBUGF(NETIF_DEBUG, ("XAxiEthernet detect_phy: PHY detected.\r\n"));
			return phy_addr;
		}
	}

	LWIP_DEBUGF(NETIF_DEBUG, ("XAxiEthernet detect_phy: No PHY detected.  Assuming a PHY at address 0\r\n"));

        /* default to zero */
	return 0;
}

unsigned get_IEEE_phy_speed(XAxiEthernet *xaxiemacp)
{
	u32 phy_addr = detect_phy(xaxiemacp);
	u16 phy_val;
	u16 phy_identifier;
	u16 phy_model;
	u16 control;
	u16 status;
	u16 partner_capabilities;
	u16 partner_capabilities_1000;
	u16 phylinkspeed;
	int TimeOut;

	/* Get the PHY Identifier and Model number */
	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, 2, &phy_identifier);
	XAxiEthernet_PhyRead(xaxiemacp, phy_addr, 3, &phy_model);
	phy_model = phy_model & PHY_MODEL_NUM_MASK;

	if ((phy_identifier == MARVEL_PHY_IDENTIFIER) &&
			(phy_model == MARVEL_PHY_88E1111_MODEL)) {
		if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
					XAE_PHY_TYPE_RGMII_2_0) {
        		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, 20, &phy_val);
			phy_val |= PHY_88E1111_RGMII_RX_CLOCK_DELAYED_MASK;
			XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, 20, phy_val);
	
        		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, 0, &control);
			XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, 0,
                                        control | IEEE_CTRL_RESET_MASK);

			TimeOut = RESET_TIMEOUT;
			while (TimeOut) {
        			XAxiEthernet_PhyRead(xaxiemacp, phy_addr, 0, &control);
				if (!(control & IEEE_CTRL_RESET_MASK))
					break;

				TimeOut -= 1;
			}

			if (!TimeOut) {
				xil_printf("%s: Phy Reset failed\n\r", __FUNCTION__);
				return 0;
			}
		} 

		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr,
				IEEE_1000_ADVERTISE_REG_OFFSET,
				ADVERTISE_1000);

		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr,
				IEEE_AUTONEGO_ADVERTISE_REG,
				ADVERTISE_100_AND_10);

		XAxiEthernet_PhyRead(xaxiemacp, phy_addr,
					IEEE_CONTROL_REG_OFFSET,
					&control);
		control |= (IEEE_CTRL_AUTONEGOTIATE_ENABLE |
					IEEE_STAT_AUTONEGOTIATE_RESTART);

		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr,
				IEEE_CONTROL_REG_OFFSET,
				control);

		/* Read PHY control and status registers is successful. */
		XAxiEthernet_PhyRead(xaxiemacp, phy_addr,
					IEEE_CONTROL_REG_OFFSET,
					&control);
		XAxiEthernet_PhyRead(xaxiemacp, phy_addr,
					IEEE_STATUS_REG_OFFSET,
					&status);

		if ((control & IEEE_CTRL_AUTONEGOTIATE_ENABLE) && (status &
					IEEE_STAT_AUTONEGOTIATE_CAPABLE)) {

			while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
				XAxiEthernet_PhyRead(xaxiemacp, phy_addr,
						IEEE_STATUS_REG_OFFSET,
						&status);
			}

			XAxiEthernet_PhyRead(xaxiemacp, phy_addr,
					IEEE_PARTNER_ABILITIES_1_REG_OFFSET,
					&partner_capabilities);

			if (status & IEEE_STAT_1GBPS_EXTENSIONS) {
				XAxiEthernet_PhyRead(xaxiemacp, phy_addr,
					IEEE_PARTNER_ABILITIES_3_REG_OFFSET,
					&partner_capabilities_1000);
				if (partner_capabilities_1000 &
						IEEE_AN3_ABILITY_MASK_1GBPS)
					return 1000;
			}

			if (partner_capabilities & IEEE_AN1_ABILITY_MASK_100MBPS)
				return 100;
			if (partner_capabilities & IEEE_AN1_ABILITY_MASK_10MBPS)
				return 10;

			xil_printf("%s: unknown PHY link speed, setting TEMAC speed to be 10 Mbps\r\n",
				__FUNCTION__);
			return 10;


		} else {

			/* Update TEMAC speed accordingly */
			if (status & IEEE_STAT_1GBPS_EXTENSIONS) {

				/* Get commanded link speed */
				phylinkspeed = control &
					IEEE_CTRL_1GBPS_LINKSPEED_MASK;

				switch (phylinkspeed) {
					case (IEEE_CTRL_LINKSPEED_1000M):
						return 1000;
					case (IEEE_CTRL_LINKSPEED_100M):
						return 100;
					case (IEEE_CTRL_LINKSPEED_10M):
						return 10;
					default:
						xil_printf("%s: unknown PHY link speed (%d), setting TEMAC speed to be 10 Mbps\r\n",
							__FUNCTION__, phylinkspeed);
						return 10;
				}

			} else {

				return (control & IEEE_CTRL_LINKSPEED_MASK) ? 100 : 10;

			}

		}
	}  else if ((phy_identifier == MARVEL_PHY_IDENTIFIER) &&
			(phy_model == MARVEL_PHY_88E1116R_MODEL)) {
#if XLWIP_CONFIG_INCLUDE_AXIETH_ON_ZYNQ == 1
		xil_printf("Start PHY autonegotiation \r\n");

		XAxiEthernet_PhyWrite(xaxiemacp,phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 2);
		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_MAC, &control);
		control |= IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK;
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_CONTROL_REG_MAC, control);

		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);

		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
		control |= IEEE_ASYMMETRIC_PAUSE_MASK;
		control |= IEEE_PAUSE_MASK;
		control |= ADVERTISE_100;
		control |= ADVERTISE_10;
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
							&control);
		control |= ADVERTISE_1000;
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
							control);

		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
							&control);
		control |= (7 << 12);	/* max number of gigabit atphy_valts */
		control |= (1 << 11);	/* enable downshift */
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
							control);

		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
		control |= IEEE_STAT_AUTONEGOTIATE_RESTART;
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		control |= IEEE_CTRL_RESET_MASK;
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

		while (1) {
			XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
			if (control & IEEE_CTRL_RESET_MASK)
				continue;
			else
				break;
		}

		xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
		while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
			sleep(1);
			XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_COPPER_SPECIFIC_STATUS_REG_2,
							&phy_val);
			if (phy_val & IEEE_AUTONEG_ERROR_MASK) {
				xil_printf("Auto negotiation error \r\n");
			}
			XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_STATUS_REG_OFFSET,
							&status);
		}

		xil_printf("autonegotiation complete \r\n");

		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_SPECIFIC_STATUS_REG,
						&partner_capabilities);
		if ( ((partner_capabilities >> 14) & 3) == 2)/* 1000Mbps */
			return 1000;
		else if ( ((partner_capabilities >> 14) & 3) == 1)/* 100Mbps */
			return 100;
		else					/* 10Mbps */
			return 10;
#endif
	} else {
		xil_printf("%s: unknown PHY link speed\r\n", __FUNCTION__);
		return 0; 
	}
}

unsigned configure_IEEE_phy_speed(XAxiEthernet *xaxiemacp, unsigned speed)
{
	u16 control;
	u32 phy_addr = detect_phy(xaxiemacp);
	u16 phy_val;

	if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
				XAE_PHY_TYPE_RGMII_2_0) {
		/* Setting Tx and Rx Delays for RGMII mode */
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0x2);

		XAxiEthernet_PhyRead(xaxiemacp, phy_addr, IEEE_CONTROL_REG_MAC, &phy_val);
		phy_val |= IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK;
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_CONTROL_REG_MAC, phy_val);

		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0x0);
	}

	XAxiEthernet_PhyRead(xaxiemacp, phy_addr,
				IEEE_CONTROL_REG_OFFSET,
				&control);
	control &= ~IEEE_CTRL_LINKSPEED_1000M;
	control &= ~IEEE_CTRL_LINKSPEED_100M;
	control &= ~IEEE_CTRL_LINKSPEED_10M;

	if (speed == 1000) {
		control |= IEEE_CTRL_LINKSPEED_1000M;
	}

	else if (speed == 100) {
		control |= IEEE_CTRL_LINKSPEED_100M;
		/* Dont advertise PHY speed of 1000 Mbps */
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr,
					IEEE_1000_ADVERTISE_REG_OFFSET,
					0);
		/* Dont advertise PHY speed of 10 Mbps */
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr,
				IEEE_AUTONEGO_ADVERTISE_REG,
				ADVERTISE_100);

	}
	else if (speed == 10) {
		control |= IEEE_CTRL_LINKSPEED_10M;
		/* Dont advertise PHY speed of 1000 Mbps */
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr,
				IEEE_1000_ADVERTISE_REG_OFFSET,
					0);
		/* Dont advertise PHY speed of 100 Mbps */
		XAxiEthernet_PhyWrite(xaxiemacp, phy_addr,
				IEEE_AUTONEGO_ADVERTISE_REG,
				ADVERTISE_10);
	}

	XAxiEthernet_PhyWrite(xaxiemacp, phy_addr,
				IEEE_CONTROL_REG_OFFSET,
				control | IEEE_CTRL_RESET_MASK);

	if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
			XAE_PHY_TYPE_SGMII) {
		control &= (~PHY_R0_ISOLATE);
		XAxiEthernet_PhyWrite(xaxiemacp,
				XPAR_AXIETHERNET_0_SGMII_PHYADDR,
				IEEE_CONTROL_REG_OFFSET,
				control | IEEE_CTRL_AUTONEGOTIATE_ENABLE);
	}

	{
		volatile int wait;
		for (wait=0; wait < 100000; wait++);
		for (wait=0; wait < 100000; wait++);
	}
	return 0;
}

unsigned Phy_Setup (XAxiEthernet *xaxiemacp)
{
	unsigned link_speed = 1000;

	if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
						XAE_PHY_TYPE_RGMII_1_3) {
		; /* Add PHY initialization code for RGMII 1.3 */
	} else if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
						XAE_PHY_TYPE_RGMII_2_0) {
		; /* Add PHY initialization code for RGMII 2.0 */
	} else if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
						XAE_PHY_TYPE_SGMII) {
#ifdef  CONFIG_LINKSPEED_AUTODETECT
		u32 phy_wr_data = IEEE_CTRL_AUTONEGOTIATE_ENABLE |
					IEEE_CTRL_LINKSPEED_1000M;
		phy_wr_data &= (~PHY_R0_ISOLATE);

		XAxiEthernet_PhyWrite(xaxiemacp,
				XPAR_AXIETHERNET_0_SGMII_PHYADDR,
				IEEE_CONTROL_REG_OFFSET,
				phy_wr_data);
#endif
	} else if (XAxiEthernet_GetPhysicalInterface(xaxiemacp) ==
						XAE_PHY_TYPE_1000BASE_X) {
		; /* Add PHY initialization code for 1000 Base-X */
	}
/* set PHY <--> MAC data clock */
#ifdef  CONFIG_LINKSPEED_AUTODETECT
	link_speed = get_IEEE_phy_speed(xaxiemacp);
	xil_printf("auto-negotiated link speed: %d\r\n", link_speed);
#elif	defined(CONFIG_LINKSPEED1000)
	link_speed = 1000;
	configure_IEEE_phy_speed(xaxiemacp, link_speed);
	xil_printf("link speed: %d\r\n", link_speed);
#elif	defined(CONFIG_LINKSPEED100)
	link_speed = 100;
	configure_IEEE_phy_speed(xaxiemacp, link_speed);
	xil_printf("link speed: %d\r\n", link_speed);
#elif	defined(CONFIG_LINKSPEED10)
	link_speed = 10;
	configure_IEEE_phy_speed(xaxiemacp, link_speed);
	xil_printf("link speed: %d\r\n", link_speed);
#endif
	return link_speed;
}
