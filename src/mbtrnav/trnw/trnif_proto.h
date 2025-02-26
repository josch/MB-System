///
/// @file trnif_proto.h
/// @authors k. headley
/// @date 18 oct 2019

/// TRN netif protocol
/// trnif read, handle, pub

/// @sa doxygen-examples.c for more examples of Doxygen markup


/////////////////////////
// Terms of use
/////////////////////////
/*
 Copyright Information
  
 Copyright 2002-2019 MBARI
 Monterey Bay Aquarium Research Institute, all rights reserved.
 
 Terms of Use
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version. You can access the GPLv3 license at
 http://www.gnu.org/licenses/gpl-3.0.html
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details
 (http://www.gnu.org/licenses/gpl-3.0.html)
 
 MBARI provides the documentation and software code "as is", with no warranty,
 express or implied, as to the software, title, non-infringement of third party
 rights, merchantability, or fitness for any particular purpose, the accuracy of
 the code, or the performance or results which you may obtain from its use. You
 assume the entire risk associated with use of the code, and you agree to be
 responsible for the entire cost of repair or servicing of the program with
 which you are using the code.
 
 In no event shall MBARI be liable for any damages, whether general, special,
 incidental or consequential damages, arising out of your use of the software,
 including, but not limited to, the loss or corruption of your data or damages
 of any kind resulting from use of the software, any prohibited use, or your
 inability to use the software. You agree to defend, indemnify and hold harmless
 MBARI and its officers, directors, and employees against any claim, loss,
 liability or expense, including attorneys' fees, resulting from loss of or
 damage to property or the injury to or death of any person arising out of the
 use of the software.
 
 The MBARI software is provided without obligation on the part of the
 Monterey Bay Aquarium Research Institute to assist in its use, correction,
 modification, or enhancement.
 
 MBARI assumes no responsibility or liability for any third party and/or
 commercial software required for the database or applications. Licensee agrees
 to obtain and maintain valid licenses for any additional third party software
 required.
 */

#ifndef TRNIF_PROTO_H
#define TRNIF_PROTO_H

/////////////////////////
// Includes 
/////////////////////////

#include "netif.h"
#include "trnw.h"
#include "msocket.h"

/////////////////////////
// Macros
/////////////////////////
#define MBIF_MSG_SIZE 16
#define TRNX_MSG_SIZE sizeof(trnuif_msg_t)
#define TRNX_MSG_SBUF_BYTES 16
#define TRNX_MSG_DA_LEN 6

#define PROTO_MB_CON  "CON"
#define PROTO_MB_REQ  "REQ"
#define PROTO_MB_ACK  "ACK"
#define PROTO_MB_NACK  "NACK"

#define PROTO_TRNU_CON  "CON"
#define PROTO_TRNU_REQ  "REQ"
#define PROTO_TRNU_RST  "RST"
#define PROTO_TRNU_HBT  "HBT"
#define PROTO_TRNU_PING "PING"
#define PROTO_TRNU_DIS  "DIS"
#define PROTO_TRNU_ACK  "ACK"
#define PROTO_TRNU_NACK "NAK"
#define PROTO_TRNU_RST_OFS  "RST_OFS"
#define PROTO_TRNU_RST_BOX  "RST_BOX"

// These macros create new messages using malloc
// caller must relese using free
#define TRNU_MSG_CON()  trnu_msg_str(PROTO_TRNU_CON)
#define TRNU_MSG_REQ()  trnu_msg_str(PROTO_TRNU_REQ)
#define TRNU_MSG_RST()  trnu_msg_str(PROTO_TRNU_RST)
#define TRNU_MSG_HBT()  trnu_msg_str(PROTO_TRNU_HBT)
#define TRNU_MSG_PING() trnu_msg_str(PROTO_TRNU_PING)
#define TRNU_MSG_DIS()  trnu_msg_str(PROTO_TRNU_DIS)
#define TRNU_MSG_ACK()  trnu_msg_str(PROTO_TRNU_ACK)
#define TRNU_MSG_NACK() trnu_msg_str(PROTO_TRNU_NACK)
#define TRNU_MSG_RST_OFS(x,y,z) trnu_msg_d3(PROTO_TRNU_RST_OFS,x,y,z)
#define TRNU_MSG_RST_BOX(x,y,z,sx,sy,sz) trnu_msg_d6(PROTO_TRNU_RST_BOX,x,y,z,sx,sy,sz)
#define TRNU_MSG_PD(id, n, pd) trnu_msg_pd(id,n,pd)

#define PROTO_MSG_LEN(s) (strlen(s)+1)

/////////////////////////
// Type Definitions
/////////////////////////
// resource bundle points to
// objects/data needed by message handlers, etc.
typedef struct trnif_res_s{
    wtnav_t *trn;
}trnif_res_t;

typedef int (*trnu_reset_callback_fn)(void);
typedef int (*trnu_reset_ofs_callback_fn)(double ofs_x, double ofs_y, double ofs_z);
typedef int (*trnu_reset_box_callback_fn)(double ofs_x, double ofs_y, double ofs_z, double sd_x, double sd_y, double sd_z);

typedef struct trnuif_res_s{
    wtnav_t *trn;
    trnu_reset_callback_fn reset_callback;
    trnu_reset_ofs_callback_fn reset_ofs_callback;
    trnu_reset_box_callback_fn reset_box_callback;
}trnuif_res_t;

#pragma pack(push,1)
typedef struct trnuif_msg_s
{
    char mid[TRNX_MSG_SBUF_BYTES];
    double dvals[6];
}trnuif_msg_t;
#pragma pack(pop)

/////////////////////////
// Exports
/////////////////////////

#ifdef __cplusplus
extern "C" {
#endif
    int trnif_msg_read_ct(byte **pdest, uint32_t *len, netif_t *self, msock_connection_t *peer, int *errout);
    int trnif_msg_handle_ct(void *msg, netif_t *self, msock_connection_t *peer, int *errout);
    
    int trnif_msg_read_mb(byte **pdest, uint32_t *len, netif_t *self, msock_connection_t *peer, int *errout);
    int trnif_msg_handle_mb(void *msg, netif_t *self, msock_connection_t *peer, int *errout);
    int trnif_msg_pub_mb(netif_t *self, msock_connection_t *peer, char *data, size_t len);
    
    int trnif_msg_read_trnu(byte **pdest, uint32_t *len, netif_t *self, msock_connection_t *peer, int *errout);
    int trnif_msg_handle_trnu(void *msg, netif_t *self, msock_connection_t *peer, int *errout);
    int trnif_msg_pub_trnu(netif_t *self, msock_connection_t *peer, char *data, size_t len);
    trnuif_msg_t *trnu_msg_str(const char *mid);
    trnuif_msg_t *trnu_msg_pd(const char *mid, int n, double *darr);
    trnuif_msg_t *trnu_msg_d3(const char *mid, double d0, double d1, double d2);
    trnuif_msg_t *trnu_msg_d6(const char *mid, double d0, double d1, double d2, double d3, double d4, double d5);

    int trnif_msg_read_trnmsg(byte **pdest, uint32_t *len, netif_t *self, msock_connection_t *peer, int *errout);
    int trnif_msg_handle_trnmsg(void *msg, netif_t *self, msock_connection_t *peer, int *errout);
    
    int trnif_msg_pub(netif_t *self, msock_connection_t *peer, char *data, size_t len);
#ifdef __cplusplus
}
#endif

#endif // THIS_FILE
