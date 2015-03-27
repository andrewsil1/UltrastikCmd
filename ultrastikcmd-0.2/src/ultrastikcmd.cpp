//============================================================================
// Name        : ultrastikcmd.cpp
// Author      : Aleph (aleph.at.al3ph.org)
// Version 0.2 : Andy Silverman (andrewsi.at.outlook.com)
// Email	   : andrewsi.at.outlook.com
// Version     : 0.2.0
// Copyright   : Silicon Based Life (2009)
// Description : UltraStik Controller Setup for Linux
//============================================================================

/*
	This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#define HAVE_STDBOOL_H

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
#include <hid.h>
#include <usb.h>
#include <memory.h>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

// extra headers for the bind_usb call below...
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>

int bind_usb(std::string &, int &);

using namespace std;

const char *argp_program_version = PACKAGE_STRING;
const char *argp_program_bug_address = PACKAGE_BUGREPORT;

/* Program documentation. */
static char doc[] = "ultrastikcmd -- a program to write ultrastik configs";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/* The options we understand. */
static struct argp_option options[] = {
	{"verbose",  	'v', 0,      		0,  "Produce verbose output" },
	{"quiet",    	'q', 0,      		0,  "Don't produce any output" },
	{"silent",   	's', 0,      		OPTION_ALIAS },
	{"controller",	'c', "[1,2,3,4]", 	0,  "Select controller to modify" },
	{"id",			'i', "[1,2,3,4]", 	0,  "Change controller id" },
	{"border",		'b', "[0-255,0-255,...]", 	0,  "Set Map border 8 comma separated values." },
	{"row",			'r', "[?],...", 	0,  "Set Map row, repeat 9 times. Analog (-), Center (C), Up (N), UPRight (NE), Right (E),DownRight (SE)"
			", Down (S), DownLeft (SW), Left (W), UPLeft (NW),Sticky (*)" },
	{"restrictor",	'o', 0, 			0,  "Use restrictor" },
	{"flash",		'f', 0, 			0,  "Write config to flash." },
	{"ultramap",	'u', "path-to-file", 	0,  "Loads config from Ultramap .um file." },

	{ 0 }
};

/* Enum for keeping track of stick type found */
enum Stiktype {NEW, OLD};

/* Used by main to communicate with parse_opt. */
struct arguments {
	int silent, verbose;
	int controller;
	int change_controller;
	unsigned char border[8];
	int border_set;
	unsigned char matrix[9*9];
	int rowcount;
	int flash;
	int restrictor;
};

unsigned char const SEND_PACKET_LEN = 4; // For new firmware sticks, Reports are output in chunks of 4 bytes.

void trim_spaces( string& str) {
	// Trim Both leading and trailing spaces
	size_t startpos = str.find_first_not_of(" \t\r\n"); // Find the first character position after excluding leading blank spaces
	size_t endpos = str.find_last_not_of(" \t\r\n"); // Find the first character position from reverse af

	// if all spaces or empty return an empty string
	if(( string::npos == startpos ) || ( string::npos == endpos)) {
		str = "";
	} else {
		str = str.substr( startpos, endpos-startpos+1 );
	}
}

unsigned char convert_mode_char(char *val) {
	unsigned char ret = 0x00;

	if(!strcasecmp(val,"-")) ret = 0x00;
	if(!strcasecmp(val,"C")) ret = 0x01;
	if(!strcasecmp(val,"N")) ret = 0x02;
	if(!strcasecmp(val,"NE")) ret = 0x03;
	if(!strcasecmp(val,"E")) ret = 0x04;
	if(!strcasecmp(val,"SE")) ret = 0x05;
	if(!strcasecmp(val,"S")) ret = 0x06;
	if(!strcasecmp(val,"SW")) ret = 0x07;
	if(!strcasecmp(val,"W")) ret = 0x08;
	if(!strcasecmp(val,"NW")) ret = 0x09;
	if(!strcasecmp(val,"*")) ret = 0x0A;

	return ret;
}

int load_ultramap(char *path,struct arguments *arguments) {
	int ret = 0;
	string line;
	ifstream is(path); // open a file stream
	bool foundmagic = false;
	int mapsize = 0;

	if ( is.is_open() ) {
		while (getline(is, line)) {
			trim_spaces(line);

			if(line.empty())
				continue;

			// MapFileFormatVersion=1.0
			if(!foundmagic) {
				if(string::npos == line.find("MapFileFormatVersion=1.0")) {
					printf("Invalid .um file, \"MapFileFormatVersion=1.0\" not found.\n");
					exit(1);
				}
				foundmagic = true;
				continue;
			}

			// MapSize=9
			if(!mapsize){
				size_t pos = line.find("MapSize=");
				if(string::npos == pos) {
					printf("MapSize not found.\n");
					exit(1);
				}
				sscanf(line.c_str()+8,"%d",&mapsize);
				if( mapsize != 9 ) {
					printf("Invalid MapSize [%d], only MapSize 9 supported.\n",mapsize);
					exit(1);
				}
				continue;
			}

			// MapBorderLocations=30,58,86,114,142,170,198,226
			if(!arguments->border_set && string::npos != line.find("MapBorderLocations=")){
				char *str = strdup(line.c_str()+strlen("MapBorderLocations="));
				if(char *p = strtok(str,",")){
					int val = 0;
					int i = 0;
					if(sscanf(p,"%d",&val)){
						arguments->border_set = 1;
						arguments->border[i] = val;

						for(i=1;i<8 && (p = strtok(NULL,","));i++){
							if(sscanf(p,"%d",&val)){
								arguments->border[i] = val;
							}
						}
					}
				}
				free(str);
			}

			// MapRow1=W,W,W,C,C,C,E,E,E
			// MapRow2=W,W,W,C,C,C,E,E,E
			// MapRow3=W,W,W,C,C,C,E,E,E
			// MapRow4=W,W,W,C,C,C,E,E,E
			// MapRow5=W,W,W,C,C,C,E,E,E
			// MapRow6=W,W,W,C,C,C,E,E,E
			// MapRow7=W,W,W,C,C,C,E,E,E
			// MapRow8=W,W,W,C,C,C,E,E,E
			// MapRow9=W,W,W,C,C,C,E,E,E
			std::stringstream rowcount;
			rowcount << arguments->rowcount+1;

			if(arguments->rowcount < 9 && string::npos != line.find(string("MapRow")+rowcount.str()+string("="))){
				char *str = strdup(line.c_str()+strlen("MapRowX="));

				if(char *p = strtok(str,",")){
					int i = 0;
					arguments->matrix[(arguments->rowcount*9)+i] = convert_mode_char(p);
					for(i=1;i<9 && (p = strtok(NULL,","));i++){
						arguments->matrix[(arguments->rowcount*9)+i] = convert_mode_char(p);
					}
				}

				arguments->rowcount++;

				free(str);
			}
		}

		if (!is.eof()) {
			//the input wasn't fully read
		}

	} else {
		printf("failed to open file: %s\n",path);
		exit(1);
	}

	return ret;
}

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
	/* Get the input argument from argp_parse, which we
	 know is a pointer to our arguments structure. */
	struct arguments *arguments = (struct arguments *)state->input;

	switch (key) {
		case 'q': case 's':
			arguments->silent = 1;
			break;

		case 'u':
			load_ultramap(arg,arguments);
			break;

		case 'v':
			arguments->verbose = 1;
			break;

		case 'c':
			sscanf(arg,"%d",&(arguments->controller));
			break;

		case 'i':
			sscanf(arg,"%d",&(arguments->change_controller));
			break;

		case 'f':
			arguments->flash = 1;
			break;

		case 'o':
			arguments->restrictor = 1;
			break;

		case 'b':
			if(char *p = strtok(arg,",")){
				int val = 0;
				int i = 0;
				if(sscanf(p,"%d",&val)){
					arguments->border_set = 1;
					arguments->border[i] = val;

					for(i=1;i<8 && (p = strtok(NULL,","));i++){
						if(sscanf(p,"%d",&val)){
							arguments->border[i] = val;
						}
					}
				}
			}
			break;

		case 'r':
			if(arguments->rowcount<9){
				if(char *p = strtok(arg,",")){
					int i = 0;
					arguments->matrix[(arguments->rowcount*9)+i] = convert_mode_char(p);
					for(i=1;i<9 && (p = strtok(NULL,","));i++){
						arguments->matrix[(arguments->rowcount*9)+i] = convert_mode_char(p);
					}
				}

				arguments->rowcount++;
			}
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

// This function outputs 4-byte chunks to the new firmware HID Output report on interface 3, rather than sending
// control messages to endpoint 0.
void hid_out_report(HIDInterface* hid, char packet[], bool verbose) {
	unsigned char const PATHLEN = 2;
	int const PATH_OUT[PATHLEN] = {0x00010000, 0x0005004b}; //Follow HID path to output report descriptor
	// These magic numbers are constructed from the "lsusb -v" HID descriptor parse report, and the technique
	// for constructing the path is in the libhid_test.c sample from the API. (Usage #'s and Usage Page #'s)

	int ret = hid_set_output_report(hid, PATH_OUT, PATHLEN, packet, SEND_PACKET_LEN);
	if (ret != HID_RET_SUCCESS) {
		fprintf(stderr, "hid_set_output_report failed with return code %d\n", ret);
	} else if (verbose) {
			fprintf(stderr, "Sent: ");
			for (int i=0; i < 4; i++) {
			 	fprintf(stderr, "0x%02x ",packet[i]);
			}
			fprintf(stderr,"\n");
	}
}


/* argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

#define VENDOR_ID_ULTIMARC	0xD209
#define PRODUCT_ID_ULTRASTIK_NEW 0x0511 // Newer Ultrastiks report a different PID and use HID output to program the maps.
#define PRODUCT_ID_ULTRASTIK1 0x0501

int main(int argc, char **argv) {
	HIDInterface* hid;
	hid_return ret;

	struct arguments arguments;

	Stiktype stikfound;

    /* Default values. */
	arguments.silent = 0;
	arguments.verbose = 0;
	arguments.controller = 1;
	arguments.change_controller = 0;
	arguments.border_set = 1; //Some map files are missing the border values and then things get confused. Set our defaults to avoid this.
	arguments.border[0] = 30; arguments.border[1] = 58;
	arguments.border[2] = 86; arguments.border[3] = 114;
	arguments.border[4] = 142; arguments.border[5] = 170;
	arguments.border[6] = 198; arguments.border[7] = 226;
	arguments.rowcount = 0;
	memset(arguments.matrix,0,sizeof(arguments.matrix));
	arguments.restrictor = 0;
	arguments.flash = 0;

	/* Parse our arguments; every option seen by parse_opt will
	   be reflected in arguments. */
	argp_parse (&argp, argc, argv, 0, 0, &arguments);

	if( arguments.verbose ) {
		printf("Write to flash: %s\n",(arguments.flash ? "Yes" : "No"));
		printf("Use restrictor: %s\n",(arguments.restrictor ? "Yes" : "No"));
	}

	if(arguments.border_set && arguments.verbose){
		printf("Border locations::\n");
		for(unsigned int i=0;i<sizeof(arguments.border);i++){
			printf(" 0x%02x ",(unsigned char)arguments.border[i]);
		}
		printf("\n");
	}

	if(arguments.rowcount && arguments.verbose){
		printf("Joystick control map:\n");
		for(unsigned int y=0;y<9;y++){
			for(unsigned int x=0;x<9;x++){
				printf(" 0x%02x ",(unsigned char)arguments.matrix[y*9+x]);
			}
			printf("\n");
		}
	}

	HIDInterfaceMatcher matcher = { VENDOR_ID_ULTIMARC, PRODUCT_ID_ULTRASTIK1+arguments.controller-1, NULL, NULL, 0 };

	hid_set_debug(HID_DEBUG_ERRORS);
//	hid_set_debug(HID_DEBUG_ALL); // Uncomment this to make logging extra verbose.
	hid_set_debug_stream(stderr);
	hid_set_usb_debug(0);

	ret = hid_init();
	if (ret != HID_RET_SUCCESS) {
	  	fprintf(stderr, "hid_init failed with return code %d\n", ret);
	  	return 1;
	}

	hid = hid_new_HIDInterface();
	if (hid == 0) {
	  	fprintf(stderr, "hid_new_HIDInterface() failed, out of memory?\n");
	  	return 1;
	}

	ret = hid_force_open(hid, 0, &matcher, 3);
	if (ret != HID_RET_SUCCESS) {
		//Try to find a new-flavor Ultrastik using the alternate PID instead.
		matcher.product_id = PRODUCT_ID_ULTRASTIK_NEW+arguments.controller-1;
		ret = hid_force_open(hid, 2, &matcher, 3);  // New sticks must use HID interface 2, not 0.
		if (ret != HID_RET_SUCCESS) {
			fprintf(stderr, "hid_force_open failed with return code %d\n", ret);
			return 1;
		} else {
			stikfound = NEW;
		}

	} else {
		stikfound = OLD;
	}

	if(arguments.verbose){
		ret = hid_write_identification(stdout, hid);
		if (ret != HID_RET_SUCCESS) {
			fprintf(stderr, "hid_write_identification failed with return code %d\n", ret);
			return 1;
		}
	}
	// ********************Change Ultrastik ID Transfer*********************
	// 1 "Data Write" Transfer of 32 Bytes
	// First Byte determines Ultrastik ID
	// 0x51 = Ultrastik Device #1
	// 0x52 = Ultrastik Device #2
	// 0x53 = Ultrastik Device #3
	// 0x54 = Ultrastik Device #4

	if(arguments.change_controller){
		printf("changing controller [%d] to [%d]\n",arguments.controller,arguments.change_controller);
		printf("You MUST physically disconnect and reconnect the controller afterwards.");

		char data[32];
		memset(data,0,sizeof(data));
		data[0] = 0x51+(arguments.change_controller-1);

		if (stikfound == OLD) {
			int usbret = usb_control_msg(hid->dev_handle,0x43,0xE9,0x0001, 0 , NULL, 0,1000);
			usbret = usb_control_msg(hid->dev_handle,0x43,0xEB,0x0000, 0 , data, sizeof(data),1000);
			usbret = usb_control_msg(hid->dev_handle,0xC3,0xEA,0x0000, 0 , NULL, 0 ,1000);
			usbret = usb_control_msg(hid->dev_handle,0x43,0xE9,0x0000, 0 , NULL, 0,1000);
		} else {
			// Send controller change via new style firmware
			char packet[SEND_PACKET_LEN];
			memcpy(&packet, data, SEND_PACKET_LEN); // For new firmware, send just one 4-byte report packet.
			hid_out_report(hid, packet, arguments.verbose);
		}

	} else {
		// *********************Send Map Transfer*******************************
		// This is only for Map Size = 9
		//
		// 3 "Data Write" Transfers of 32 Bytes (old stick), or 24 transfers of 4 Bytes (new stick)
		//
		// Byte[0] = 0x50 (Header)
		// Byte[1] = 0x09 (Map Size?)
		// Byte[2] = Restrictor (RestrictorOn = 0x09, RestrictorOff = 0x10)
		// Byte[3]-Byte[10] = Map Border Location
		// Byte[11]-Byte[31] = Mapping of Block Location. starting from topleft and read left to right.
		// Byte[32]-Byte[63] - Continue Mapping of Block Location
		// Byte[64]-Byte[91] - Continue Mapping of Block Location
		// Byte[92]-Byte[94] - 0x00
		// Byte[95] - (Write to Flash 0x00) (Write to RAM 0xFF, supported in Firmware 2.2)
		//
		//
		// ****************************************************
		// USB      Mapping
		// 0x00 =   Analog (-)
		// 0x01 =   Center (C)
		// 0x02 =   Up (N)
		// 0x03 =   UPRight (NE)
		// 0x04 =   Right (E)
		// 0x05 =   DownRight (SE)
		// 0x06 =   Down (S)
		// 0x07 =   DownLeft (SW)
		// 0x08 =   Left (W)
		// 0x09 =   UPLeft (NW)
		// 0x0A =   Sticky (*)

		// MapFileFormatVersion=1.0

		// MapSize=9
		// MapBorderLocations=30,58,86,114,142,170,198,226
		// MapRow1=W,W,W,C,C,C,E,E,E
		// MapRow2=W,W,W,C,C,C,E,E,E
		// MapRow3=W,W,W,C,C,C,E,E,E
		// MapRow4=W,W,W,C,C,C,E,E,E
		// MapRow5=W,W,W,C,C,C,E,E,E
		// MapRow6=W,W,W,C,C,C,E,E,E
		// MapRow7=W,W,W,C,C,C,E,E,E
		// MapRow8=W,W,W,C,C,C,E,E,E
		// MapRow9=W,W,W,C,C,C,E,E,E

		if(arguments.rowcount == 9){
			char data[96];
			memset(data,0,sizeof(data));

			data[0]  = 0x50; // Byte[0] = 0x50 (Header)
			data[1]  = 0x09; // Byte[1] = 0x09 (Map Size?)
			data[2]	 = ( arguments.restrictor ? 0x09 : 0x10 ); // Byte[2] = Restrictor (RestrictorOn = 0x09, RestrictorOff = 0x10)
			data[95] = ( arguments.flash ? 0x00 : 0xFF ); // Byte[95] - (Write to Flash 0x00) (Write to RAM 0xFF, supported in Firmware 2.2)

			if(arguments.border_set){
				// Byte[3]-Byte[10] = Map Border Location
				memcpy(&data[3],arguments.border,sizeof(arguments.border));
			}
			memcpy(&data[11],arguments.matrix,9*9);

			if (stikfound == NEW) {
				// New-style map output
				for (int i=0; i<24; i++) { //Write 96 bytes in packets of 4 bytes.
					char packet[SEND_PACKET_LEN];
					memcpy(&packet, data+(SEND_PACKET_LEN*i), SEND_PACKET_LEN);
					hid_out_report(hid, packet, arguments.verbose);
				}
			} else {
				// Old-style map output
				int usbret = usb_control_msg(hid->dev_handle,0x43,0xE9,0x0001, 0 , NULL, 0,1000);
				if (usbret < 0) {
					fprintf(stderr,"Error communicating with Ultrastik: %d\n",usbret);
					return EXIT_FAILURE;
				}

				for(int i=0;i<3;i++){ //3 packets of 32 bytes
					usbret = usb_control_msg(hid->dev_handle,0x43,0xEB,0x0000, 0 , data+(32*i), sizeof(data)/3,1000);
					if (usbret < 0) {
						fprintf(stderr,"Error communicating with Ultrastik: %d\n",usbret);
						return EXIT_FAILURE;
					}
				}

				usbret = usb_control_msg(hid->dev_handle,0xC3,0xEA,0x0000, 0 , NULL, 0 ,1000);
				if (usbret < 0) {
					fprintf(stderr,"Error communicating with Ultrastik: %d\n",usbret);
					return EXIT_FAILURE;
				}

				usbret = usb_control_msg(hid->dev_handle,0x43,0xE9,0x0000, 0 , NULL, 0,1000);
				if (usbret < 0) {
					fprintf(stderr,"Error communicating with Ultrastik: %d\n",usbret);
					return EXIT_FAILURE;
				}
			}
		}
	}

	// store the bus/dev path for binding the usb below...
	std::string fname1 =  "/dev/bus/usb/" + 
		std::string(hid->device->bus->dirname) + "/" + std::string(hid->device->filename);

	ret = hid_close(hid);
	if (ret != HID_RET_SUCCESS) {
		fprintf(stderr, "hid_close failed with return code %d\n", ret);
		return 1;
	}

	hid_delete_HIDInterface(&hid);

	ret = hid_cleanup();
	if (ret != HID_RET_SUCCESS) {
		fprintf(stderr, "hid_cleanup failed with return code %d\n", ret);
		return 1;
	}

	// rebind / connect the usb device (should recreate the js* device node file) -
	// don't do this for change_controller requests or the changes get reverted...
	if (!arguments.change_controller){
		int rc = bind_usb(fname1, arguments.verbose);
		if (rc != 0){
			return rc;
		}
	}

	return EXIT_SUCCESS;
}

// bind_usb(): bind the usb device after data has been sent...
int bind_usb(std::string &fname1, int &iVerbose){
	int rc, fd = 0;
	struct usbdevfs_ioctl usb_ioctl;

	fd = open(fname1.c_str(), O_RDWR);
	if (fd < 0) {
		int err1 = errno;
		fprintf(stderr, "Unable to open device file %s: %s\n",
				fname1.c_str(), strerror(err1));
		rc = 1;

	} else {
		usb_ioctl.ifno = 2;
		usb_ioctl.ioctl_code = USBDEVFS_CONNECT;
		usb_ioctl.data = NULL;

		rc = ioctl(fd, USBDEVFS_IOCTL, &usb_ioctl);
		if (rc == -1) {
			printf("Error in ioctl (USBDEVFS_CONNECT)");
			rc = 1;

		} else if (iVerbose){
			printf("Bind-driver request sent to the kernel\n");
		}
	}

	return rc;
}
