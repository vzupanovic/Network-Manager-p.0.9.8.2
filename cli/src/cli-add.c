#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <locale.h>

#include <glib.h>
#include <dbus/dbus-glib.h>

#include <glib/gi18n.h>
#include <nm-client.h>
#include <nm-setting-connection.h>
#include <nm-remote-settings.h>
#include <nm-connection.h>
#include <nm-setting-gsm.h>
#include <nm-setting-ip4-config.h>
#include <NetworkManager.h>
#include <nm-utils.h>
#include <nm-setting-ppp.h>
#include <nm-setting-serial.h>

#include "nmcli.h"
#include "utils.h"
#include "connections.h"
#include "devices.h"
#include "network-manager.h"
#include "cli-add.h"

#define DBUS_TYPE_G_MAP_OF_VARIANT          (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE))
#define DBUS_TYPE_G_MAP_OF_MAP_OF_VARIANT   (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, DBUS_TYPE_G_MAP_OF_VARIANT))

#define NOT_SET "not_set"

static void
usage_add (void)
{
	fprintf (stderr,
	         _("Usage: nmcli add { help | COMMAND }\n"
	         "  COMMAND := id <id> param1 <value1> param2 <value2> ... param_n <value_n>\n"
	         "  Available parameters: \n"
	         " \t<id>         [has to be specified]\n"
	         " \t<auto>       [autoconnect] default: true (t/f)\n"
	         " \t<apn>\n"
	         " \t<pin>\n"
	         " \t<username>\n"
	         " \t<password>\n"
	         " \t<netid>      [network id]\n"
	         " \t             Force the device to register only on the specified network.\n"
	         " \t<ntype>      [network type]\n"
	         " \t             -1 : any | 0 : 3G only | 1: GPRS/EDGE only\n"
	         " \t             2: prefer 3G | 3: prefer 2G | 4: prefer 4G (LTE) \n"
		 " \t             5: 4G (LTE) only\n"
	         " \t             Note: not all devices allow network preference control!\n"
	         " \t<uuid>\n"
	         " \t<auth>       [autenthication]\n"
	         " \t             Allowed methods: EAP (t/f) | PAP (t/f) | CHAP (t/f) |\n"
	         " \t             MSCHAPv2 (t/f) | MSCHAP (t/f)\n"
	         " \t<comp>       [compression]\n" 
	         " \t             Allow BSD data compression (t/f)\n"
	         " \t             Allow Deflate data compression (t/f)\n"
	         " \t             Use TCP header compression (t/f)\n"
	         " \t<echofail>   If non-zero, instruct pppd to presume the connection to the\n"
	         " \t             peer has failed if the specified number of LCP echo-requests\n"
	         " \t             go unanswered by the peer. Must be set to non zero if used.\n"
	         " \t             [zero - default]\n"
	         " \t<echoint>    If non-zero, instruct pppd to send an LCP echo-request frame\n" 
	         " \t             to the peer every n seconds (where n is the specified value).\n"
	         " \t             [zero - default]\n"
	         " \t<enc>        [Use point-to-point encryption MPPE]:\n"
	         " \t             Require 128-bit encryption (t/f)\n"
	         " \t             Use stateful MPPE (t/f)\n"
	         " \t<sbaud>      Serial baud: [default 57600] speed to use for communication over\n" 
	         " \t             the serial port. Usually no effect.\n"
	         " \t<sbits>      Byte-width of the serial communication. Allowed values [5-8]\n"
	         " \t             [default 8]\n"
	         " \t<sparity>    Parity setting of the serial port. Either 'E' for even parity,'o'\n" 
	         " \t             for odd parity, or 'n' for no parity.\n "
	         " \t<stbits>     Number of stop bits for communication on the serial port. Either\n"
	         " \t             1 or 2. The 1 in '8n1' for example. Allowed values [1, 2]\n"
	         " \t             [default 1]\n\n"));
}

static int
add_connection (DBusGProxy *proxy, char *con_name, char *apn, char *pin, 
				char *username, char *password, int ntype, char *number,
				char *auth, char *comp, char *aut, char *netid, char *enc,
				int ecoint, int ecofail, char *uuid, int sbits, char sparity,
				int stbits, int sbaud)
{
	NMConnection *connection;
	NMSettingConnection *s_con;
	NMSettingIP4Config *s_ip4;
	NMSettingGsm *s_gsm;
	NMSettingPPP *s_ppp;
	NMSettingSerial *s_serial;
	
	char *new_con_path = NULL;
	GHashTable *hash;
	GError *error = NULL;
	
	int autoconnect = 1;
	int uuid_generated = 0;
	
	
	if ((aut != NULL) && (aut[0] != 't'))
		autoconnect = 0;
	
	connection = (NMConnection *)nm_connection_new ();
	if (connection == NULL){
		printf("Unable to allocate new connection... Sorry.\n");
		return NMC_RESULT_ERROR_CON_ADD;
	}

	s_con = (NMSettingConnection *) nm_setting_connection_new ();
	if (s_con == NULL){
		printf("Failed to allocate new %s setting... Sorry.\n",NM_SETTING_CONNECTION_SETTING_NAME);
		return NMC_RESULT_ERROR_CON_ADD;
	}
	 
	nm_connection_add_setting (connection, NM_SETTING (s_con));
	
	
	if (uuid == NULL){
		uuid = nm_utils_uuid_generate ();
		uuid_generated = 1;
	}
		
	
	/*global settings*/
	              
	g_object_set (s_con,
	              NM_SETTING_CONNECTION_ID, con_name,
	              NM_SETTING_CONNECTION_UUID, uuid,
	              NM_SETTING_CONNECTION_AUTOCONNECT, (autoconnect == 1) ? TRUE : FALSE,
	              NM_SETTING_CONNECTION_TYPE, NM_SETTING_GSM_SETTING_NAME,
	              NULL);
	              
	if(uuid_generated == 1)
		g_free (uuid);
    
	/* GSM setting */
	s_gsm = (NMSettingGsm *) nm_setting_gsm_new ();
	
	if (s_gsm == NULL){
		printf("Failed to allocate new %s setting...Sorry.\n",NM_SETTING_GSM_SETTING_NAME);
		return NMC_RESULT_ERROR_CON_ADD;
	}
	
	nm_connection_add_setting (connection, NM_SETTING (s_gsm));

	/*Network type
					    Network preference to force the device to only use 
					    specific network technologies.  The permitted values
					    are: -1: any, 0: 3G only, 1: GPRS/EDGE only, 
					    2: prefer 3G, and 3: prefer 2G, 4: prefer 4G (LTE),
                                            5: 4G (LTE) only. Note that not all 
					    devices allow network preference control.
	*/ 
					   
	g_object_set (s_gsm, 
	              NM_SETTING_GSM_NUMBER, (number == NULL) ? "*99#" : number,
	              NM_SETTING_GSM_APN, apn,
	              NM_SETTING_GSM_USERNAME, username,
	              NM_SETTING_GSM_PASSWORD, password,
	              NM_SETTING_GSM_PIN, pin,
	              NM_SETTING_GSM_NETWORK_TYPE, ntype, 
	              NM_SETTING_GSM_NETWORK_ID, netid,
	              NULL);

	/* Serial setting */
	s_serial = (NMSettingSerial *) nm_setting_serial_new ();
	
	if (s_serial == NULL){
		printf("Failed to allocate new %s setting...Sorry.\n",NM_SETTING_SERIAL_SETTING_NAME);
		return NMC_RESULT_ERROR_CON_ADD;
	}
	
	nm_connection_add_setting (connection, NM_SETTING (s_serial));

	g_object_set (s_serial,
	              NM_SETTING_SERIAL_BAUD, sbaud,
	              NM_SETTING_SERIAL_BITS, sbits,
	              NM_SETTING_SERIAL_PARITY, sparity,
	              NM_SETTING_SERIAL_STOPBITS, stbits,
	              NULL);

	/* IP4 setting */
	s_ip4 = (NMSettingIP4Config *) nm_setting_ip4_config_new ();
	
	if (s_ip4 == NULL){
		printf("Failed to allocate new %s setting... Sorry.\n",NM_SETTING_IP4_CONFIG_SETTING_NAME);
		return NMC_RESULT_ERROR_CON_ADD;
	}
	
	nm_connection_add_setting (connection, NM_SETTING (s_ip4));

	g_object_set (s_ip4,
	              NM_SETTING_IP4_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_AUTO,
	              NULL);

	/* PPP setting */
	s_ppp = (NMSettingPPP *) nm_setting_ppp_new ();
	
	if (s_ppp == NULL){
		printf("Failed to allocate new %s setting... Sorry.\n", NM_SETTING_PPP_SETTING_NAME);
		return NMC_RESULT_ERROR_CON_ADD;
	}
	
	
	g_object_set(s_ppp,
				NM_SETTING_PPP_REFUSE_EAP, (auth[0] == 't') ? FALSE : TRUE,
				NM_SETTING_PPP_REFUSE_PAP, (auth[1] == 't') ? FALSE : TRUE,
				NM_SETTING_PPP_REFUSE_CHAP, (auth[2] == 't') ? FALSE : TRUE,
				NM_SETTING_PPP_REFUSE_MSCHAP, (auth[3] == 't') ? FALSE : TRUE,
				NM_SETTING_PPP_REFUSE_MSCHAPV2, (auth[4] == 't') ? FALSE : TRUE,
				NM_SETTING_PPP_NOBSDCOMP, (comp[0] == 't') ? FALSE : TRUE,
				NM_SETTING_PPP_NODEFLATE, (comp[1] == 't') ? FALSE : TRUE,
				NM_SETTING_PPP_NO_VJ_COMP,(comp[2] == 't') ? FALSE : TRUE, //tcp header compression
				NM_SETTING_PPP_REQUIRE_MPPE, (strcmp(enc, "ff")==0) ? FALSE : TRUE,
				NM_SETTING_PPP_MPPE_STATEFUL, (enc[1] == 't') ? TRUE : FALSE,
				NM_SETTING_PPP_REQUIRE_MPPE_128, (enc[0] == 't') ? TRUE : FALSE,
				NM_SETTING_PPP_LCP_ECHO_FAILURE, (ecofail > 0) ? ecofail : 0,
				NM_SETTING_PPP_LCP_ECHO_INTERVAL, (ecoint > 0) ? ecoint: 0,
				NULL);

	nm_connection_add_setting (connection, NM_SETTING (s_ppp));
	
	hash = nm_connection_to_hash (connection, NM_SETTING_HASH_FLAG_ALL);

	/* Call AddConnection with the hash as argument */
	if (!dbus_g_proxy_call (proxy, "AddConnection", &error,
	                        DBUS_TYPE_G_MAP_OF_MAP_OF_VARIANT, hash,
	                        G_TYPE_INVALID,
	                        DBUS_TYPE_G_OBJECT_PATH, &new_con_path,
	                        G_TYPE_INVALID)) {
		g_print ("Error adding connection: %s %s\n",
		         dbus_g_error_get_name (error),
		         error->message);
		g_clear_error (&error);
		g_hash_table_destroy (hash);
		g_object_unref (connection);
		return NMC_RESULT_ERROR_CON_ADD;
	} else {
		g_print ("\n\tConnection added successfully at: %s \n\tUse: nmcli con list id %s - to see connection detailed info \n\tcon delete id %s - to delete connection\n", 
		         new_con_path, con_name, con_name);
		g_print ("\tTo bring connection up use: nmcli con up id %s.\n",con_name);
		g_print ("\tTo see connection status use: nmcli con status id %s.\n\n",con_name);
		g_free (new_con_path);
	}

	g_hash_table_destroy (hash);
	g_object_unref (connection);
	
	return 0;
}
	

NMCResultCode
do_add (NmCli *nmc, int argc, char **argv)
{
	DBusGConnection *bus;
	DBusGProxy *proxy;

	GError *err = NULL;
	
	int i;
	
	char *apn = NULL;
	char *pin = NULL;
	char *username = NULL;
	char *password = NULL;
	char *number = NULL; 
	char *ntype = NULL;
	char *auth = NULL;
	char *comp = NULL;
	char *aut = NULL;
	char *netid = NULL;
	char *enc = NULL;
	char *echofail = NULL;
	char *echoint = NULL;
	char *uuid = NULL;
	char *sbits = NULL;
	char *stbits = NULL;
	char *sparity = NULL;
	char *sbaud = NULL;
	
	if ((*argv == NULL) || strcmp(argv[0],"help") == 0 || strcmp(argv[0],"-help") == 0){
		usage_add();
	}
		
	else{
		nmc->return_value = NMC_RESULT_ERROR_CON_ADD;
		if (!nmc_is_nm_running (nmc, &err)) {
			if (err) {
				g_string_printf (nmc->return_text, _("Error: Can't find out if NetworkManager is running: %s."), err->message);
				nmc->return_value = NMC_RESULT_ERROR_UNKNOWN;
				g_error_free (err);
				return nmc->return_value;
			} else {
				g_string_printf (nmc->return_text, _("Error: NetworkManager is not running."));
				nmc->return_value = NMC_RESULT_ERROR_NM_NOT_RUNNING;
				return nmc->return_value;
			}
		}

		if (matches(argv[0],"id") != 0){
			g_string_printf (nmc->return_text, _("Error: id has to be specified."));
			nmc->return_value = NMC_RESULT_ERROR_USER_INPUT;
			return nmc->return_value;
		}
		else if (matches(argv[0],"id") == 0){
			if(argc == 1){
				g_string_printf (nmc->return_text, _("Error: argument missing for parameter id."));	
				nmc->return_value = NMC_RESULT_ERROR_USER_INPUT;
				return nmc->return_value;
			}
			else if (argc % 2!=0){
				g_string_printf (nmc->return_text, _("Error: Some arguments are missing."));
				nmc->return_value = NMC_RESULT_ERROR_USER_INPUT;
				return nmc->return_value;
			}
		}
		
		
		for (i=0; i<argc; i=i+2){
			
			(matches(argv[i], "apn") == 0) ? apn = argv[i + 1] : NOT_SET;	
			(matches(argv[i], "pin") == 0) ? pin = argv[i + 1] : NOT_SET;
			(matches(argv[i], "username") == 0) ? username = argv[i + 1] : NOT_SET;
			(matches(argv[i], "password") == 0) ? password = argv[i + 1] : NOT_SET;
			(matches(argv[i], "ntype") == 0) ? ntype = argv[i + 1] : NOT_SET;
			(matches(argv[i], "number") == 0) ? number = argv[i + 1] : NOT_SET;
			(matches(argv[i], "auth") == 0) ? auth = argv[i + 1] : NOT_SET;
			(matches(argv[i], "comp") == 0) ? comp = argv[i + 1] : NOT_SET;
			(matches(argv[i], "auto") == 0) ? aut = argv[i + 1] : NOT_SET;
			(matches(argv[i], "netid") == 0) ? netid = argv[i + 1] : NOT_SET;
			(matches(argv[i], "enc") == 0) ? enc = argv[i + 1] : NOT_SET;
			(matches(argv[i], "echoint") == 0) ? echoint = argv[i + 1] : NOT_SET;
			(matches(argv[i], "echofail") == 0) ? echofail = argv[i + 1] : NOT_SET; 
			(matches(argv[i], "uuid") == 0) ? uuid = argv[i + 1] : NOT_SET;
			(matches(argv[i], "sbits") == 0) ? sbits = argv[i + 1] : NOT_SET;
			(matches(argv[i], "sparity") == 0) ? sparity = argv[i + 1] : NOT_SET;
			(matches(argv[i], "stbits") == 0) ? stbits = argv[i + 1] : NOT_SET;
			(matches(argv[i], "sbaud") == 0) ? sbaud = argv[i + 1] : NOT_SET;
			
		}
		
			
		g_type_init ();

		bus = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);

		proxy = dbus_g_proxy_new_for_name (bus,
					       NM_DBUS_SERVICE,
	                                       NM_DBUS_PATH_SETTINGS,
	                                       NM_DBUS_IFACE_SETTINGS);
	                                   

		if (add_connection (proxy, argv[1], apn, pin, username, password, ((ntype == NULL) ? -1 : atoi(ntype)), 
		                    number, ((auth == NULL) ? "fffff" : auth), ((comp == NULL) ? "fff" : comp), aut,
		                    netid, ((enc == NULL) ? "ff" : enc), ((echoint == NULL) ? 0 : atoi(echoint)),
		                    ((echofail == NULL) ? 0 : atoi(echofail)), uuid, ((sbits == NULL) ? 8 : atoi(sbits)), 
		                    ((sparity!=NULL) ? sparity[0] : 110), ((stbits == NULL) ? 1 : atoi(stbits)),
		                    ((sbaud == NULL) ? 57600 : atoi(sbaud))) != 0){
			fprintf (stderr,"Error: unable to add new connection.");
			nmc->return_value = NMC_RESULT_ERROR_CON_ADD;
			nmc->return_text = g_string_new (_("Fail"));
			g_object_unref (proxy);
			dbus_g_connection_unref (bus);
			return nmc->return_value;
		}

		g_object_unref (proxy);
		dbus_g_connection_unref (bus);
		nmc->return_value = NMC_RESULT_SUCCESS;
	}

	
	return nmc->return_value;
}

