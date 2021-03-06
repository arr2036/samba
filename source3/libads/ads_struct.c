/* 
   Unix SMB/CIFS implementation.
   ads (active directory) utility library
   Copyright (C) Andrew Tridgell 2001
   Copyright (C) Andrew Bartlett 2001

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "ads.h"

/* return a ldap dn path from a string, given separators and field name
   caller must free
*/
char *ads_build_path(const char *realm, const char *sep, const char *field, int reverse)
{
	char *p, *r;
	int numbits = 0;
	char *ret;
	int len;
	char *saveptr;

	r = SMB_STRDUP(realm);

	if (!r || !*r) {
		return r;
	}

	for (p=r; *p; p++) {
		if (strchr(sep, *p)) {
			numbits++;
		}
	}

	len = (numbits+1)*(strlen(field)+1) + strlen(r) + 1;

	ret = (char *)SMB_MALLOC(len);
	if (!ret) {
		free(r);
		return NULL;
	}

	if (strlcpy(ret,field, len) >= len) {
		/* Truncate ! */
		free(r);
		free(ret);
		return NULL;
	}
	p=strtok_r(r, sep, &saveptr);
	if (p) {
		if (strlcat(ret, p, len) >= len) {
			free(r);
			free(ret);
			return NULL;
		}

		while ((p=strtok_r(NULL, sep, &saveptr)) != NULL) {
			int retval;
			char *s = NULL;
			if (reverse)
				retval = asprintf(&s, "%s%s,%s", field, p, ret);
			else
				retval = asprintf(&s, "%s,%s%s", ret, field, p);
			free(ret);
			if (retval == -1) {
				free(r);
				return NULL;
			}
			ret = SMB_STRDUP(s);
			free(s);
		}
	}

	free(r);
	return ret;
}

/* return a dn of the form "dc=AA,dc=BB,dc=CC" from a 
   realm of the form AA.BB.CC 
   caller must free
*/
char *ads_build_dn(const char *realm)
{
	return ads_build_path(realm, ".", "dc=", 0);
}

/* return a DNS name in the for aa.bb.cc from the DN  
   "dc=AA,dc=BB,dc=CC".  caller must free
*/
char *ads_build_domain(const char *dn)
{
	char *dnsdomain = NULL;

	/* result should always be shorter than the DN */

	if ( (dnsdomain = SMB_STRDUP( dn )) == NULL ) {
		DEBUG(0,("ads_build_domain: malloc() failed!\n"));		
		return NULL;		
	}	

	if (!strlower_m( dnsdomain )) {
		SAFE_FREE(dnsdomain);
		return NULL;
	}

	all_string_sub( dnsdomain, "dc=", "", 0);
	all_string_sub( dnsdomain, ",", ".", 0 );

	return dnsdomain;	
}



#ifndef LDAP_PORT
#define LDAP_PORT 389
#endif

/*
  initialise a ADS_STRUCT, ready for some ads_ ops
*/
ADS_STRUCT *ads_init(const char *realm, 
		     const char *workgroup,
		     const char *ldap_server,
		     enum ads_sasl_state_e sasl_state)
{
	ADS_STRUCT *ads;
	int wrap_flags;

	ads = SMB_XMALLOC_P(ADS_STRUCT);
	ZERO_STRUCTP(ads);
#ifdef HAVE_LDAP
	ads_zero_ldap(ads);
#endif

	ads->server.realm = realm? SMB_STRDUP(realm) : NULL;
	ads->server.workgroup = workgroup ? SMB_STRDUP(workgroup) : NULL;
	ads->server.ldap_server = ldap_server? SMB_STRDUP(ldap_server) : NULL;

	/* the caller will own the memory by default */
	ads->is_mine = 1;

	wrap_flags = lp_client_ldap_sasl_wrapping();
	if (wrap_flags == -1) {
		wrap_flags = 0;
	}

	switch (sasl_state) {
	case ADS_SASL_PLAIN:
		break;
	case ADS_SASL_SIGN:
		wrap_flags |= ADS_AUTH_SASL_SIGN;
		break;
	case ADS_SASL_SEAL:
		wrap_flags |= ADS_AUTH_SASL_SEAL;
		break;
	}

	ads->auth.flags = wrap_flags;

	/* Start with the configured page size when the connection is new,
	 * we will drop it by half we get a timeout.   */
	ads->config.ldap_page_size     = lp_ldap_page_size();

	return ads;
}

/****************************************************************
****************************************************************/

bool ads_set_sasl_wrap_flags(ADS_STRUCT *ads, unsigned flags)
{
	unsigned other_flags;

	if (!ads) {
		return false;
	}

	other_flags = ads->auth.flags & ~(ADS_AUTH_SASL_SIGN|ADS_AUTH_SASL_SEAL);

	ads->auth.flags = flags | other_flags;

	return true;
}

/*
  free the memory used by the ADS structure initialized with 'ads_init(...)'
*/
void ads_destroy(ADS_STRUCT **ads)
{
	if (ads && *ads) {
		bool is_mine;

		is_mine = (*ads)->is_mine;
#ifdef HAVE_LDAP
		ads_disconnect(*ads);
#endif
		SAFE_FREE((*ads)->server.realm);
		SAFE_FREE((*ads)->server.workgroup);
		SAFE_FREE((*ads)->server.ldap_server);

		SAFE_FREE((*ads)->auth.realm);
		SAFE_FREE((*ads)->auth.password);
		SAFE_FREE((*ads)->auth.user_name);
		SAFE_FREE((*ads)->auth.kdc_server);
		SAFE_FREE((*ads)->auth.ccache_name);

		SAFE_FREE((*ads)->config.realm);
		SAFE_FREE((*ads)->config.bind_path);
		SAFE_FREE((*ads)->config.ldap_server_name);
		SAFE_FREE((*ads)->config.server_site_name);
		SAFE_FREE((*ads)->config.client_site_name);
		SAFE_FREE((*ads)->config.schema_path);
		SAFE_FREE((*ads)->config.config_path);

		ZERO_STRUCTP(*ads);
#ifdef HAVE_LDAP
		ads_zero_ldap(*ads);
#endif

		if ( is_mine )
			SAFE_FREE(*ads);
	}
}
