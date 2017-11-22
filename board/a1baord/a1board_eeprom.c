/*
 * Copyright (C) 2015 Variscite Ltd. All Rights Reserved.
 * Maintainer: Ron Donio <ron.d@variscite.com>
 * Configuration settings for the Variscite VAR_SOM_MX6 and DART6UL board.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <common.h>
#include <i2c.h>

#include "mx6var_v2_eeprom.h"

static u32 get_address_by_index(unsigned char index);
static u32 get_value_by_index(unsigned char index);
static int handle_one_command(struct eeprom_command_type *eeprom_commands,int command_num);

/*
 * DDR Register struct
 * The eeprom contains structure of
 * 1 byte index in this table,
 * 1 byte index to common values in the next tableto write to this address.
 * If there is some new addresses got from the calibration program
 * they should be added in the end of the array.
 * The maximum array size is 256 addresses.
 */
static const u32 rom_addresses[]=
	{
		#include "addresses.inc"
	};

static const u32 rom_values[] =
	{
		#include "values.inc"
	};

static u32 ram_addresses[MAXIMUM_RAM_ADDRESSES] __attribute__ ((section ("sram")));
static u32 ram_values[MAXIMUM_RAM_VALUES] __attribute__ ((section ("sram")));

void load_custom_data(u32 *custom_addresses_values)
{
	int i;

	for(i=0;i<MAXIMUM_RAM_ADDRESSES;i++)
	{
		ram_addresses[i]=0;
		ram_values[i]=0;
	}

	int j=0;
	for(i=0;i<32;i++)
	{
		if(custom_addresses_values[i]==0)
			break;
		ram_addresses[j]=custom_addresses_values[i];
		j++;
	}

	i++;
	if(i>MAXIMUM_RAM_ADDRESSES)
		return;

	j=0;
	for(;i<32;i++)
	{
		if(custom_addresses_values[i]==0)
			break;
		ram_values[j]=custom_addresses_values[i];
		j++;
	}
}

int handle_eeprom_data(struct var_eeprom_config_struct_v2_type *var_eeprom_config_struct_v2)
{
	load_custom_data(var_eeprom_config_struct_v2->custom_addresses_values);

	return setup_ddr_parameters(var_eeprom_config_struct_v2->eeprom_commands);
}

int setup_ddr_parameters(struct eeprom_command_type *eeprom_commands)
{
	int i=0;

	while(i<MAXIMUM_COMMANDS_NUMBER)
	{
		i=handle_one_command(eeprom_commands, i);
		if(i<0)
			return -1;
		if(i==0)
			return 0;
	}
	return 0;
}


static int handle_one_command(struct eeprom_command_type *eeprom_commands,int command_num)
{
	volatile u32 *data;
	u32 address;
	u32 value;
#if 0
	printf("Executing command %03d: %03d,  %03d\n",
			command_num,
			eeprom_commands[command_num].address_index,
			eeprom_commands[command_num].value_index);
#endif

	switch(eeprom_commands[command_num].address_index)
	{
		case WHILE_NOT_EQUAL_INDEX:
			command_num++;
			address=get_address_by_index(eeprom_commands[command_num].address_index);
			value=get_value_by_index(eeprom_commands[command_num].value_index);
			data=(u32*)address;
			/* printf("waiting while data at address %08x is not equal %08x\n",address,value); */
			while(data[0]!=value);

			command_num++;
			break;
		case WHILE_EQUAL_INDEX:
			command_num++;
			address=get_address_by_index(eeprom_commands[command_num].address_index);
			value=get_value_by_index(eeprom_commands[command_num].value_index);
			data=(u32*)address;
			/* printf("waiting while data at address %08x is equal %08x\n",address,value); */
			while(data[0]==value);

			command_num++;
			break;
		case WHILE_AND_INDEX:
			command_num++;
			address=get_address_by_index(eeprom_commands[command_num].address_index);
			value=get_value_by_index(eeprom_commands[command_num].value_index);
			data=(u32*)address;
			/* printf("waiting while data at address %08x and %08x is not zero\n",address,value); */
			while(data[0]&value);

			command_num++;
			break;
		case WHILE_NOT_AND_INDEX:
			command_num++;
			address=get_address_by_index(eeprom_commands[command_num].address_index);
			value=get_value_by_index(eeprom_commands[command_num].value_index);
			data=(u32*)address;
			/* printf("waiting while data at address %08x and %08x is zero\n",address,value); */
			while(!(data[0]&value));

			command_num++;
			break;
		case DELAY_10USEC_INDEX:
			/* Delay for Value * 10 uSeconds */
			/* printf("Delaying for %d microseconds\n",eeprom_commands[command_num].value_index*10); */
			p_udelay((int)(eeprom_commands[command_num].value_index*10));
			command_num++;
			break;
		case LAST_COMMAND_INDEX:
			command_num=0;
			break;
		default:
			address=get_address_by_index(eeprom_commands[command_num].address_index);
			value=get_value_by_index(eeprom_commands[command_num].value_index);
			data=(u32*)address;
			/* printf("Setting data at address %08x to %08x\n",address,value); */
			data[0]=value;
			command_num++;
			break;
	}

	return command_num;
}

static u32 get_address_by_index(unsigned char index)
{
	if(index>=MAXIMUM_ROM_ADDR_INDEX)
		return ram_addresses[index-MAXIMUM_ROM_ADDR_INDEX];

	return rom_addresses[index];
}

static u32 get_value_by_index(unsigned char index)
{
	if(index>=MAXIMUM_ROM_VALUE_INDEX)
		return ram_values[index-MAXIMUM_ROM_VALUE_INDEX];

	return rom_values[index];
}

int var_eeprom_v2_read_struct(struct var_eeprom_config_struct_v2_type *var_eeprom_config_struct_v2)
{
	int eeprom_found;
	int ret = 0;
	int oldbus;

	oldbus = i2c_get_bus_num();
	i2c_set_bus_num(1);

	eeprom_found = i2c_probe(VARISCITE_MX6_EEPROM_CHIP);
	if (0 == eeprom_found)
	{
		if (i2c_read(VARISCITE_MX6_EEPROM_CHIP, 0, 1, (void*)var_eeprom_config_struct_v2,sizeof(struct var_eeprom_config_struct_v2_type)))
		{
			printf("Read device ID error!\n");
			return -1;
		}
	}
	else
		printf("Error! Couldn't find EEPROM device\n");

	i2c_set_bus_num(oldbus);
	return ret;
}

int var_eeprom_write(uchar *ptr, u32 size, u32 offset)
{
	int ret = 0;
	u32 size_written;
	u32 size_to_write;
	u32 P0_select_page_EEPROM;
	u32 chip;
	u32 addr;
	int oldbus;

	oldbus = i2c_get_bus_num();
	i2c_set_bus_num(1);

	/* Write to EEPROM device */
	size_written = 0;
	size_to_write = size;
	while ((0 == ret) && (size_written < size_to_write))
	{
		P0_select_page_EEPROM = (offset > 0xFF);
		chip = VARISCITE_MX6_EEPROM_CHIP + P0_select_page_EEPROM;
		addr = (offset & 0xFF);
		ret = i2c_write(chip, addr, 1, ptr, VARISCITE_MX6_EEPROM_WRITE_MAX_SIZE);

		/* Wait for EEPROM write operation to complete (No ACK) */
		udelay(11000);

		size_written += VARISCITE_MX6_EEPROM_WRITE_MAX_SIZE;
		offset += VARISCITE_MX6_EEPROM_WRITE_MAX_SIZE;
		ptr += VARISCITE_MX6_EEPROM_WRITE_MAX_SIZE;
	}

	i2c_set_bus_num(oldbus);
	return ret;
}

/*
 * var_eeprom_params command intepreter.
 */
static int do_var_eeprom_params(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	var_eeprom_config_struct_v2_type var_eeprom_cfg;
	int offset;

	if (argc !=5)
	{
		return -1;
	}

	memset(&var_eeprom_cfg, 0x00, sizeof(var_eeprom_cfg));

	memcpy(&var_eeprom_cfg.part_number[0], argv[1], sizeof(var_eeprom_cfg.part_number));
	memcpy(&var_eeprom_cfg.Assembly[0], argv[2], sizeof(var_eeprom_cfg.Assembly));
	memcpy(&var_eeprom_cfg.date[0], argv[3], sizeof(var_eeprom_cfg.date));

	var_eeprom_cfg.som_info = (u8) simple_strtoul(argv[4], NULL, 16);

	var_eeprom_cfg.part_number[sizeof(var_eeprom_cfg.part_number)-1] = (u8)0x00;
	var_eeprom_cfg.Assembly[sizeof(var_eeprom_cfg.Assembly)-1] = (u8)0x00;
	var_eeprom_cfg.date[sizeof(var_eeprom_cfg.date)-1] = (u8)0x00;

	printf("Part number: %s\n", (char *)var_eeprom_cfg.part_number);
	printf("Assembly: %s\n", (char *)var_eeprom_cfg.Assembly);
	printf("Date of production: %s\n", (char *)var_eeprom_cfg.date);

	printf("SOM Configuration: 0x%x: ",var_eeprom_cfg.som_info);
	switch (var_eeprom_cfg.som_info & 0x3) {
	case 0x00:
		puts("SD card only");
		break;
	case 0x01:
		puts("NAND");
		break;
	case 0x02:
		puts("eMMC");
		break;
	case 0x03:
		puts("Ilegal!");
		break;
	}

	if (var_eeprom_cfg.som_info & 0x04)
		puts(", WiFi");

	switch ((var_eeprom_cfg.som_info >> 3) & 0x3) {
	case 0x0:
		puts(", SOM Rev 1\n");
		break;
	case 0x1:
		puts(", SOM Rev 2 (5G)\n");
		break;
	default:
		puts(", SOM Rev ilegal!\n");
		break;
	}

	offset = (uchar *)&var_eeprom_cfg.part_number[0] - (uchar *)&var_eeprom_cfg;
	if (var_eeprom_write((uchar *)&var_eeprom_cfg.part_number[0],
				sizeof(var_eeprom_cfg.part_number),
				VARISCITE_MX6_EEPROM_STRUCT_OFFSET + offset) )
	{
		printf("Error writing Part Number to EEPROM!\n");
		return -1;
	}

	offset = (uchar *)&var_eeprom_cfg.Assembly[0] - (uchar *)&var_eeprom_cfg;
	if (var_eeprom_write((uchar *)&var_eeprom_cfg.Assembly[0],
				sizeof(var_eeprom_cfg.Assembly),
				VARISCITE_MX6_EEPROM_STRUCT_OFFSET + offset) )
	{
		printf("Error writing Assembly to EEPROM!\n");
		return -1;
	}

	offset = (uchar *)&var_eeprom_cfg.date[0] - (uchar *)&var_eeprom_cfg;
	if (var_eeprom_write((uchar *)&var_eeprom_cfg.date[0],
				sizeof(var_eeprom_cfg.date),
				VARISCITE_MX6_EEPROM_STRUCT_OFFSET + offset) )
	{
		printf("Error writing date to EEPROM!\n");
		return -1;
	}

	offset = (uchar *)&var_eeprom_cfg.som_info - (uchar *)&var_eeprom_cfg;
	if (var_eeprom_write((uchar *)&var_eeprom_cfg.som_info,
				sizeof(var_eeprom_cfg.som_info),
				VARISCITE_MX6_EEPROM_STRUCT_OFFSET + offset) )
	{
		printf("Error writing Som Info to EEPROM!\n");
		return -1;
	}

	printf("EEPROM updated successfully\n");

	return 0;
}

U_BOOT_CMD(
	vareeprom,	5,	1,	do_var_eeprom_params,
	"",
	""
);
