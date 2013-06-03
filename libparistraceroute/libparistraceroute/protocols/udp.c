#include <stdlib.h>           // malloc()
#include <string.h>           // memcpy()
#include <stdbool.h>          // bool
#include <errno.h>            // ERRNO, EINVAL
#include <stddef.h>           // offsetof()
#include <netinet/udp.h>      // udphdr
#include <netinet/in.h>       // IPPROTO_UDP = 17

#include "../protocol.h"      // csum
#include "ipv4_pseudo_header.h"
#include "ipv6_pseudo_header.h"

#define UDP_DEFAULT_SRC_PORT 2828
#define UDP_DEFAULT_DST_PORT 2828

#define UDP_FIELD_SRC_PORT   "src_port"
#define UDP_FIELD_DST_PORT   "dst_port"
#define UDP_FIELD_LENGTH     "length"
#define UDP_FIELD_CHECKSUM   "checksum"

// XXX mandatory fields ?
// XXX UDP parsing missing

// BSD/Linux abstraction
#ifdef __FAVOR_BSD
#   define SRC_PORT uh_sport
#   define DST_PORT uh_dport
#   define LENGTH   uh_ulen
#   define CHECKSUM uh_sum
#else
#   define SRC_PORT source 
#   define DST_PORT dest 
#   define LENGTH   len
#   define CHECKSUM check 
#endif

/**
 * UDP fields
 */

static protocol_field_t udp_fields[] = {
    {
        .key = UDP_FIELD_SRC_PORT,
        .type = TYPE_INT16,
        .offset = offsetof(struct udphdr, SRC_PORT),
    }, {
        .key = UDP_FIELD_DST_PORT,
        .type = TYPE_INT16,
        .offset = offsetof(struct udphdr, DST_PORT),
    }, {
        .key = UDP_FIELD_LENGTH,
        .type = TYPE_INT16,
        .offset = offsetof(struct udphdr, LENGTH),
    }, {
        .key = UDP_FIELD_CHECKSUM,
        .type = TYPE_INT16,
        .offset = offsetof(struct udphdr, CHECKSUM),
        // optional = 0
    },
    END_PROTOCOL_FIELDS
};

/**
 * Default UDP values
 */

static struct udphdr udp_default = {
    .SRC_PORT = UDP_DEFAULT_SRC_PORT,
    .DST_PORT = UDP_DEFAULT_DST_PORT,
    .LENGTH   = 0,
    .CHECKSUM = 0
};


/**
 * \brief Retrieve the number of fields in a UDP header
 * \return The number of fields
 */

size_t udp_get_num_fields(void) {
    return sizeof(udp_fields) / sizeof(protocol_field_t);
}

/**
 * \brief Retrieve the size of an UDP header 
 * \param udp_header Address of an UDP header or NULL
 * \return The size of an UDP header
 */

size_t udp_get_header_size(const uint8_t * udp_header) {
    return sizeof(struct udphdr);
}

/**
 * \brief Write the default UDP header
 * \param udp_header The address of an allocated buffer that will
 *    store the UDP header or NULL.
 * \return The size of the default header.
 */

size_t udp_write_default_header(uint8_t * udp_header) {
    size_t size = sizeof(struct udphdr);
    if (udp_header) memcpy(udp_header, &udp_default, size);
    return size;
}

/**
 * \brief Compute and write the checksum related to an UDP header
 * \param udp_header Points to the begining of the UDP header and its content.
 *    The UDP checksum stored in this header is updated by this function.
 * \param ip_psh The IP layer part of the pseudo header. This buffer should
 *    contain the content of an ipv4_pseudo_header_t or an ipv6_pseudo_header_t
 *    structure.
 * \sa http://www.networksorcery.com/enp/protocol/udp.htm#Checksum
 * \return true if everything is fine, false otherwise  
 */

bool udp_write_checksum(uint8_t * udp_header, buffer_t * ip_psh)
{
    struct udphdr * udp_hdr = (struct udphdr *) udp_header;
    uint8_t       * psh;
    size_t          size_psh;

    // UDP checksum requires an part of the IP header
    if (!ip_psh) {
        errno = EINVAL;
        return false;
    }

    // Allocate the buffer which will contains the pseudo header
    size_psh = ntohs(udp_hdr->LENGTH) + buffer_get_size(ip_psh);
    if (!(psh = malloc(size_psh * sizeof(uint8_t)))) {
        return false;
    }

    // Put the excerpt of the IP header into the pseudo header
    memcpy(psh, buffer_get_data(ip_psh), buffer_get_size(ip_psh));

    // Put the UDP header and its content into the pseudo header
    memcpy(psh + buffer_get_size(ip_psh), udp_hdr, ntohs(udp_hdr->LENGTH));//sizeof(struct udphdr));

    // Overrides the UDP checksum in psh with zeros
    // Checksum debug: http://www4.ncsu.edu/~mlsichit/Teaching/407/Resources/udpChecksum.html
    memset(psh + buffer_get_size(ip_psh) + offsetof(struct udphdr, CHECKSUM), 0, sizeof(uint16_t));

    // Compute the checksum
    udp_hdr->check = csum((const uint16_t *) psh, size_psh);
    free(psh);
    return true;
}

buffer_t * udp_create_pseudo_header(const uint8_t * ip_segment)
{
    // TODO dispatch IPv4 and IPv6 header
    // http://www.networksorcery.com/enp/protocol/udp.htm#Checksum
    // XXX IPv6 hacks -> todo generic.
    /* buffer_t *psh;
    unsigned char ip_version = buffer_guess_ip_version(ip_segment); 
   
       psh = ip_version == 6 ? udp_create_psh_ipv6(ip_segment) :
                        == 4 ? udp_create_psh_ipv4(ip_segment) :
                        NULL;
       if(!psh){
           perror("E:can not create udp pseudo header");
       }*/
    return ipv4_pseudo_header_create(ip_segment);
}

static protocol_t udp = {
    .name                 = "udp",
    .protocol             = IPPROTO_UDP, 
    .get_num_fields       = udp_get_num_fields,
    .write_checksum       = udp_write_checksum,
    .create_pseudo_header = udp_create_pseudo_header,
    .fields               = udp_fields,
  //.defaults             = udp_defaults,             // XXX used when generic
    .write_default_header = udp_write_default_header, // TODO generic
  //.socket_type          = NULL,
    .get_header_size      = udp_get_header_size,
};

PROTOCOL_REGISTER(udp);
