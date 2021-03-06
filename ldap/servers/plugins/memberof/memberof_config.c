/** BEGIN COPYRIGHT BLOCK
 * This Program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2 of the License.
 * 
 * This Program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along with
 * this Program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA.
 * 
 * In addition, as a special exception, Red Hat, Inc. gives You the additional
 * right to link the code of this Program with code not covered under the GNU
 * General Public License ("Non-GPL Code") and to distribute linked combinations
 * including the two, subject to the limitations in this paragraph. Non-GPL Code
 * permitted under this exception must only link to the code of this Program
 * through those well defined interfaces identified in the file named EXCEPTION
 * found in the source code files (the "Approved Interfaces"). The files of
 * Non-GPL Code may instantiate templates or use macros or inline functions from
 * the Approved Interfaces without causing the resulting work to be covered by
 * the GNU General Public License. Only Red Hat, Inc. may make changes or
 * additions to the list of Approved Interfaces. You must obey the GNU General
 * Public License in all respects for all of the Program code and other code used
 * in conjunction with the Program except the Non-GPL Code covered by this
 * exception. If you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you do not wish to
 * provide this exception without modification, you must delete this exception
 * statement from your version and license this file solely under the GPL without
 * exception. 
 * 
 * 
 * Copyright (C) 2008 Red Hat, Inc.
 * All rights reserved.
 * END COPYRIGHT BLOCK **/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/*
 * memberof_config.c - configuration-related code for memberOf plug-in
 *
 */

#include <plstr.h>

#include "memberof.h"

#define MEMBEROF_CONFIG_FILTER "(objectclass=*)"

/*
 * The configuration attributes are contained in the plugin entry e.g.
 * cn=MemberOf Plugin,cn=plugins,cn=config
 *
 * Configuration is a two step process.  The first pass is a validation step which
 * occurs pre-op - check inputs and error out if bad.  The second pass actually
 * applies the changes to the run time config.
 */


/*
 * function prototypes
 */ 
static int memberof_validate_config (Slapi_PBlock *pb, Slapi_Entry* entryBefore, Slapi_Entry* e, 
										 int *returncode, char *returntext, void *arg);
static int memberof_search (Slapi_PBlock *pb, Slapi_Entry* entryBefore, Slapi_Entry* e, 
								int *returncode, char *returntext, void *arg)
{
	return SLAPI_DSE_CALLBACK_OK;
}

/*
 * static variables
 */
/* This is the main configuration which is updated from dse.ldif.  The
 * config will be copied when it is used by the plug-in to prevent it
 * being changed out from under a running memberOf operation. */
static MemberOfConfig theConfig = {NULL, NULL,0, NULL, NULL, NULL, NULL};
static Slapi_RWLock *memberof_config_lock = 0;
static int inited = 0;


static int dont_allow_that(Slapi_PBlock *pb, Slapi_Entry* entryBefore, Slapi_Entry* e, 
						   int *returncode, char *returntext, void *arg)
{
	*returncode = LDAP_UNWILLING_TO_PERFORM;
	return SLAPI_DSE_CALLBACK_ERROR;
}

/*
 * memberof_config()
 *
 * Read configuration and create a configuration data structure.
 * This is called after the server has configured itself so we can
 * perform checks with regards to suffixes if it ever becomes
 * necessary.
 * Returns an LDAP error code (LDAP_SUCCESS if all goes well).
 */
int
memberof_config(Slapi_Entry *config_e, Slapi_PBlock *pb)
{
	int returncode = LDAP_SUCCESS;
	char returntext[SLAPI_DSE_RETURNTEXT_SIZE];

	if ( inited ) {
		slapi_log_error( SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
				 "only one memberOf plugin instance can be used\n" );
		return( LDAP_PARAM_ERROR );
	}

	/* initialize the RW lock to protect the main config */
	memberof_config_lock = slapi_new_rwlock();

	/* initialize fields */
	if (SLAPI_DSE_CALLBACK_OK == memberof_validate_config(NULL, NULL, config_e,
							&returncode, returntext, NULL))
	{
		memberof_apply_config(NULL, NULL, config_e, &returncode, returntext, NULL);
	}

	/*
	 * config DSE must be initialized before we get here we only need the dse callbacks
	 * for the plugin entry, but not the shared config entry.
	 */
	if (returncode == LDAP_SUCCESS) {
		const char *config_dn = slapi_sdn_get_dn(memberof_get_plugin_area());
		slapi_config_register_callback_plugin(SLAPI_OPERATION_MODIFY, DSE_FLAG_PREOP | DSE_FLAG_PLUGIN,
			config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
			memberof_validate_config, NULL,pb);
		slapi_config_register_callback_plugin(SLAPI_OPERATION_MODIFY, DSE_FLAG_POSTOP | DSE_FLAG_PLUGIN,
			config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
			memberof_apply_config, NULL, pb);
		slapi_config_register_callback_plugin(SLAPI_OPERATION_MODRDN, DSE_FLAG_PREOP | DSE_FLAG_PLUGIN,
			config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
			dont_allow_that, NULL, pb);
		slapi_config_register_callback_plugin(SLAPI_OPERATION_DELETE, DSE_FLAG_PREOP | DSE_FLAG_PLUGIN,
			config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
			dont_allow_that, NULL, pb);
		slapi_config_register_callback_plugin(SLAPI_OPERATION_SEARCH, DSE_FLAG_PREOP | DSE_FLAG_PLUGIN,
			config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
			memberof_search, NULL, pb);
	}

	inited = 1;

	if (returncode != LDAP_SUCCESS) {
		slapi_log_error(SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
				"Error %d: %s\n", returncode, returntext);
        }

	return returncode;
}

void
memberof_release_config()
{
	const char *config_dn = slapi_sdn_get_dn(memberof_get_plugin_area());

	slapi_config_remove_callback(SLAPI_OPERATION_MODIFY, DSE_FLAG_PREOP,
		config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
		memberof_validate_config);
	slapi_config_remove_callback(SLAPI_OPERATION_MODIFY, DSE_FLAG_POSTOP,
		config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
		memberof_apply_config);
	slapi_config_remove_callback(SLAPI_OPERATION_MODRDN, DSE_FLAG_PREOP,
		config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
		dont_allow_that);
	slapi_config_remove_callback(SLAPI_OPERATION_DELETE, DSE_FLAG_PREOP,
		config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
		dont_allow_that);
	slapi_config_remove_callback(SLAPI_OPERATION_SEARCH, DSE_FLAG_PREOP,
		config_dn, LDAP_SCOPE_BASE, MEMBEROF_CONFIG_FILTER,
		memberof_search);

	slapi_destroy_rwlock(memberof_config_lock);
	memberof_config_lock = NULL;
	inited = 0;
}

/*
 * memberof_validate_config()
 *
 * Validate the pending changes in the e entry.
 */
static int 
memberof_validate_config (Slapi_PBlock *pb, Slapi_Entry* entryBefore, Slapi_Entry* e, 
	int *returncode, char *returntext, void *arg)
{
	Slapi_Attr *memberof_attr = NULL;
	Slapi_Attr *group_attr = NULL;
	Slapi_DN *config_sdn = NULL;
	char *syntaxoid = NULL;
	char *config_dn = NULL;
	int not_dn_syntax = 0;

	*returncode = LDAP_UNWILLING_TO_PERFORM; /* be pessimistic */

	/* Make sure both the group attr and the memberOf attr
	 * config atributes are supplied.  We don't care about &attr
	 * here, but slapi_entry_attr_find() requires us to pass it. */
	if (!slapi_entry_attr_find(e, MEMBEROF_GROUP_ATTR, &group_attr) &&
		!slapi_entry_attr_find(e, MEMBEROF_ATTR, &memberof_attr))
	{
		Slapi_Attr *test_attr = NULL;
		Slapi_Value *value = NULL;
		int hint = 0;

		/* Loop through each group attribute to see if the syntax is correct. */
		hint = slapi_attr_first_value(group_attr, &value);
		while (value && (not_dn_syntax == 0))
		{
			/* We need to create an attribute to find the syntax. */
			test_attr = slapi_attr_new();
			slapi_attr_init(test_attr, slapi_value_get_string(value));

			/* Get the syntax OID and see if it's the Distinguished Name or
			 * Name and Optional UID syntax. */
			slapi_attr_get_syntax_oid_copy(test_attr, &syntaxoid );
			not_dn_syntax = strcmp(syntaxoid, DN_SYNTAX_OID) & strcmp(syntaxoid, NAME_OPT_UID_SYNTAX_OID);
			slapi_ch_free_string(&syntaxoid);

			/* Print an error if the current attribute is not using the Distinguished
			 * Name syntax, otherwise get the next group attribute. */
			if (not_dn_syntax)
			{
				PR_snprintf(returntext, SLAPI_DSE_RETURNTEXT_SIZE,
					"The %s configuration attribute must be set to "
					"an attribute defined to use either the Distinguished "
					"Name or Name and Optional UID syntax. (illegal value: %s)",
					slapi_value_get_string(value), MEMBEROF_GROUP_ATTR);
			}
			else
			{
				hint = slapi_attr_next_value(group_attr, hint, &value);
			}

			/* Free the group attribute. */
			slapi_attr_free(&test_attr);
		}

		if (not_dn_syntax == 0)
		{
			/* Check the syntax of the memberof attribute. */
			slapi_attr_first_value(memberof_attr, &value);
			test_attr = slapi_attr_new();
			slapi_attr_init(test_attr, slapi_value_get_string(value));
			slapi_attr_get_syntax_oid_copy(test_attr, &syntaxoid );
			not_dn_syntax = strcmp(syntaxoid, DN_SYNTAX_OID);
			slapi_ch_free_string(&syntaxoid);
			slapi_attr_free(&test_attr);

			if (not_dn_syntax)
			{
				PR_snprintf(returntext, SLAPI_DSE_RETURNTEXT_SIZE,
					"The %s configuration attribute must be set to "
					"an attribute defined to use the Distinguished "
					"Name syntax.  (illegal value: %s)",
					slapi_value_get_string(value), MEMBEROF_ATTR);
				goto done;
			}
			else
			{
				*returncode = LDAP_SUCCESS;
			}
		}
	} else {
		PR_snprintf(returntext, SLAPI_DSE_RETURNTEXT_SIZE,
			"The %s and %s configuration attributes must be provided",
			MEMBEROF_GROUP_ATTR, MEMBEROF_ATTR); 
		goto done;
	}

	if ((config_dn = slapi_entry_attr_get_charptr(e, SLAPI_PLUGIN_SHARED_CONFIG_AREA))){
		/* Now check the shared config attribute, validate it now */

		Slapi_Entry *e = NULL;
		int rc = 0;

		rc = slapi_dn_syntax_check(pb, config_dn, 1);
		if (rc) { /* syntax check failed */
			slapi_log_error( SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM, "memberof_validate_config: "
					"%s does not contain a valid DN (%s)\n",
					SLAPI_PLUGIN_SHARED_CONFIG_AREA, config_dn);
			*returncode = LDAP_INVALID_DN_SYNTAX;
			goto done;
		}
		config_sdn = slapi_sdn_new_dn_byval(config_dn);

		slapi_search_internal_get_entry(config_sdn, NULL, &e, memberof_get_plugin_id());
		if(e){
			slapi_entry_free(e);
			*returncode = LDAP_SUCCESS;
		} else {
			/* config area does not exist! */
			PR_snprintf(returntext, SLAPI_DSE_RETURNTEXT_SIZE,
								"The %s configuration attribute points to an entry that  "
								"can not be found.  (%s)",
								SLAPI_PLUGIN_SHARED_CONFIG_AREA, config_dn);
			*returncode = LDAP_UNWILLING_TO_PERFORM;
		}
	}

done:
	slapi_sdn_free(&config_sdn);
	slapi_ch_free_string(&config_dn);

	if (*returncode != LDAP_SUCCESS)
	{
		return SLAPI_DSE_CALLBACK_ERROR;
	}
	else
	{
		return SLAPI_DSE_CALLBACK_OK;
	}
}


/*
 * memberof_apply_config()
 *
 * Apply the pending changes in the e entry to our config struct.
 * memberof_validate_config()  must have already been called.
 */
int
memberof_apply_config (Slapi_PBlock *pb, Slapi_Entry* entryBefore, Slapi_Entry* e, 
	int *returncode, char *returntext, void *arg)
{
	Slapi_Entry *config_entry = NULL;
	Slapi_DN *config_sdn = NULL;
	char **groupattrs = NULL;
	char *memberof_attr = NULL;
	char *filter_str = NULL;
	int num_groupattrs = 0;
	int groupattr_name_len = 0;
	char *allBackends = NULL;
	char *entryScope = NULL;
        char *entryScopeExcludeSubtree = NULL;
	char *sharedcfg = NULL;

	*returncode = LDAP_SUCCESS;

	/*
	 * Apply the config settings from the shared config entry
	 */
	sharedcfg = slapi_entry_attr_get_charptr(e, SLAPI_PLUGIN_SHARED_CONFIG_AREA);
	if(sharedcfg){
		int rc = 0;

		rc = slapi_dn_syntax_check(pb, sharedcfg, 1);
		if (rc) { /* syntax check failed */
			slapi_log_error( SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,"memberof_apply_config: "
					"%s does not contain a valid DN (%s)\n",
					SLAPI_PLUGIN_SHARED_CONFIG_AREA, sharedcfg);
			*returncode = LDAP_INVALID_DN_SYNTAX;
			goto done;
		}
		if((config_sdn = slapi_sdn_new_dn_byval(sharedcfg))){
			slapi_search_internal_get_entry(config_sdn, NULL, &config_entry, memberof_get_plugin_id());
			if(config_entry){
				char errtext[SLAPI_DSE_RETURNTEXT_SIZE];
				int err = 0;
				/*
				 * If we got here, we are updating the shared config area, so we need to
				 * validate and apply the settings from that config area.
				 */
				if ( SLAPI_DSE_CALLBACK_ERROR == memberof_validate_config (pb, NULL, config_entry, &err, errtext,0))
				{
					slapi_log_error( SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
									"%s", errtext);
					*returncode = LDAP_UNWILLING_TO_PERFORM;
					goto done;

				}
				e = config_entry;
			} else {
				/* this should of been checked in preop validation */
				slapi_log_error( SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM, "memberof_apply_config: "
						"Failed to locate shared config entry (%s)\n",sharedcfg);
				*returncode = LDAP_UNWILLING_TO_PERFORM;
				goto done;
			}
		}
	}

	groupattrs = slapi_entry_attr_get_charray(e, MEMBEROF_GROUP_ATTR);
	memberof_attr = slapi_entry_attr_get_charptr(e, MEMBEROF_ATTR);
	allBackends = slapi_entry_attr_get_charptr(e, MEMBEROF_BACKEND_ATTR);
	entryScope = slapi_entry_attr_get_charptr(e, MEMBEROF_ENTRY_SCOPE_ATTR);
        entryScopeExcludeSubtree = slapi_entry_attr_get_charptr(e, MEMBEROF_ENTRY_SCOPE_EXCLUDE_SUBTREE);

	/* We want to be sure we don't change the config in the middle of
	 * a memberOf operation, so we obtain an exclusive lock here */
	memberof_wlock_config();

	if (groupattrs)
	{
		int i = 0;

		slapi_ch_array_free(theConfig.groupattrs);
		theConfig.groupattrs = groupattrs;
		groupattrs = NULL; /* config now owns memory */

		/* We allocate a list of Slapi_Attr using the groupattrs for
		 * convenience in our memberOf comparison functions */
		for (i = 0; theConfig.group_slapiattrs && theConfig.group_slapiattrs[i]; i++)
		{
			slapi_attr_free(&theConfig.group_slapiattrs[i]);
		}

		/* Count the number of groupattrs. */
		for (num_groupattrs = 0; theConfig.groupattrs && theConfig.groupattrs[num_groupattrs]; num_groupattrs++)
		{
			/* Add up the total length of all attribute names.  We need
			 * to know this for building the group check filter later. */
			groupattr_name_len += strlen(theConfig.groupattrs[num_groupattrs]);
		}

		/* Realloc the list of Slapi_Attr if necessary. */
		if (i < num_groupattrs)
		{
			theConfig.group_slapiattrs = (Slapi_Attr **)slapi_ch_realloc((char *)theConfig.group_slapiattrs,
							sizeof(Slapi_Attr *) * (num_groupattrs + 1));
		}

		/* Build the new list */
		for (i = 0; theConfig.groupattrs[i]; i++)
		{
			theConfig.group_slapiattrs[i] = slapi_attr_new();
			slapi_attr_init(theConfig.group_slapiattrs[i], theConfig.groupattrs[i]);
		}

		/* Terminate the list. */
		theConfig.group_slapiattrs[i] = NULL;

		/* The filter is based off of the groupattr, so we
		 * update it here too. */
		slapi_filter_free(theConfig.group_filter, 1);

		if (num_groupattrs > 1)
		{
			int bytes_out = 0;
			int filter_str_len = groupattr_name_len + (num_groupattrs * 4) + 4;

			/* Allocate enough space for the filter */
			filter_str = slapi_ch_malloc(filter_str_len);

			/* Add beginning of filter. */
			bytes_out = snprintf(filter_str, filter_str_len - bytes_out, "(|");

			/* Add filter section for each groupattr. */
			for (i = 0; theConfig.groupattrs[i]; i++)
			{
				bytes_out += snprintf(filter_str + bytes_out, filter_str_len - bytes_out, "(%s=*)", theConfig.groupattrs[i]);
			}

			/* Add end of filter. */
			snprintf(filter_str + bytes_out, filter_str_len - bytes_out, ")");
		}
		else
		{
			filter_str = slapi_ch_smprintf("(%s=*)", theConfig.groupattrs[0]);
		}

		/* Log an error if we were unable to build the group filter for some
		 * reason.  If this happens, the memberOf plugin will not be able to
		 * check if an entry is a group, causing it to not catch changes.  This
		 * shouldn't happen, but there may be some garbage configuration that
		 * could trigger this. */
		if ((theConfig.group_filter = slapi_str2filter(filter_str)) == NULL)
		{
			slapi_log_error( SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
				"Unable to create the group check filter.  The memberOf "
				"plug-in will not operate on changes to groups.  Please check "
				"your %s configuration settings. (filter: %s)\n",
				MEMBEROF_GROUP_ATTR, filter_str );
		}

		slapi_ch_free_string(&filter_str);
	}

	if (memberof_attr)
	{
		slapi_ch_free_string(&theConfig.memberof_attr);
		theConfig.memberof_attr = memberof_attr;
		memberof_attr = NULL; /* config now owns memory */
	}

	if (allBackends)
	{
		if(strcasecmp(allBackends,"on")==0){
			theConfig.allBackends = 1;
		} else {
			theConfig.allBackends = 0;
		}
	} else {
		theConfig.allBackends = 0;
	}

	slapi_sdn_free(&theConfig.entryScope);
	if (entryScope)
	{
        	if (slapi_dn_syntax_check(NULL, entryScope, 1) == 1) {
            		slapi_log_error(SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
                		"Error: Ignoring invalid DN used as plugin entry scope: [%s]\n",
                		entryScope);
			theConfig.entryScope = NULL;
			slapi_ch_free_string(&entryScope);
		} else {
			theConfig.entryScope = slapi_sdn_new_dn_passin(entryScope);
		}
	} else {
		theConfig.entryScope = NULL;
	}
        
        slapi_sdn_free(&theConfig.entryScopeExcludeSubtree);
        if (entryScopeExcludeSubtree)
	{
        	if (theConfig.entryScope == NULL) {
                        slapi_log_error(SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
                		"Error: Ignoring ExcludeSubtree (%s) because entryScope is not define\n",
                		entryScopeExcludeSubtree);
			theConfig.entryScopeExcludeSubtree = NULL;
			slapi_ch_free_string(&entryScopeExcludeSubtree);
                } else if (slapi_dn_syntax_check(NULL, entryScopeExcludeSubtree, 1) == 1) {
            		slapi_log_error(SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
                		"Error: Ignoring invalid DN used as plugin entry exclude subtree: [%s]\n",
                		entryScopeExcludeSubtree);
			theConfig.entryScopeExcludeSubtree = NULL;
			slapi_ch_free_string(&entryScopeExcludeSubtree);
		} else {
			theConfig.entryScopeExcludeSubtree = slapi_sdn_new_dn_passin(entryScopeExcludeSubtree);
		}
	} else {
		theConfig.entryScopeExcludeSubtree = NULL;
	}
        if (theConfig.entryScopeExcludeSubtree && theConfig.entryScope && !slapi_sdn_issuffix(theConfig.entryScopeExcludeSubtree, theConfig.entryScope)) {
                slapi_log_error(SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
                        "Error: Ignoring ExcludeSubtree (%s) that is out of the scope (%s)\n",
                        slapi_sdn_get_dn(theConfig.entryScopeExcludeSubtree),
                        slapi_sdn_get_dn(theConfig.entryScope));
                slapi_sdn_free(&theConfig.entryScopeExcludeSubtree);
	}

	/* release the lock */
	memberof_unlock_config();

done:
	slapi_ch_free_string(&sharedcfg);
	slapi_sdn_free(&config_sdn);
	if(config_entry){
		/* we switched the entry pointer to the shared config entry - which needs to be freed */
		slapi_entry_free(e);
	}
	slapi_ch_array_free(groupattrs);
	slapi_ch_free_string(&memberof_attr);
	slapi_ch_free_string(&allBackends);

	if (*returncode != LDAP_SUCCESS)
	{
		return SLAPI_DSE_CALLBACK_ERROR;
	}
	else
	{
		return SLAPI_DSE_CALLBACK_OK;
	}
}

/*
 * memberof_copy_config()
 *
 * Makes a copy of the config in src.  This function will free the
 * elements of dest if they already exist.  This should only be called
 * if you hold the memberof config lock if src was obtained with
 * memberof_get_config().
 */
void
memberof_copy_config(MemberOfConfig *dest, MemberOfConfig *src)
{
	if (dest && src)
	{
		/* Check if the copy is already up to date */
		if (src->groupattrs)
		{
			int i = 0, j = 0;

			/* Copy group attributes string list. */
			slapi_ch_array_free(dest->groupattrs);
			dest->groupattrs = slapi_ch_array_dup(src->groupattrs);

			/* Copy group check filter. */
			slapi_filter_free(dest->group_filter, 1);
			dest->group_filter = slapi_filter_dup(src->group_filter);

			/* Copy group attributes Slapi_Attr list.
			 * First free the old list. */
			for (i = 0; dest->group_slapiattrs && dest->group_slapiattrs[i]; i++)
			{
				slapi_attr_free(&dest->group_slapiattrs[i]);
			}

			/* Count how many values we have in the source list. */
			for (j = 0; src->group_slapiattrs[j]; j++)
			{
				/* Do nothing. */
			}

			/* Realloc dest if necessary. */
			if (i < j)
			{
				dest->group_slapiattrs = (Slapi_Attr **)slapi_ch_realloc((char *)dest->group_slapiattrs, sizeof(Slapi_Attr *) * (j + 1));
			}

			/* Copy the attributes. */
			for (i = 0; src->group_slapiattrs[i]; i++)
			{
				dest->group_slapiattrs[i] = slapi_attr_dup(src->group_slapiattrs[i]);
			}

			/* Terminate the array. */
			dest->group_slapiattrs[i] = NULL;
		}

		if (src->memberof_attr)
		{
			slapi_ch_free_string(&dest->memberof_attr);
			dest->memberof_attr = slapi_ch_strdup(src->memberof_attr);
		}

		if(src->allBackends)
		{
			dest->allBackends = src->allBackends;
		}
	}
}

/*
 * memberof_free_config()
 *
 * Free's the contents of a config structure.
 */
void
memberof_free_config(MemberOfConfig *config)
{
	if (config)
	{
		int i = 0;

		slapi_ch_array_free(config->groupattrs);
		slapi_filter_free(config->group_filter, 1);

		for (i = 0; config->group_slapiattrs && config->group_slapiattrs[i]; i++)
		{
			slapi_attr_free(&config->group_slapiattrs[i]);
		}
		slapi_ch_free((void **)&config->group_slapiattrs);

		slapi_ch_free_string(&config->memberof_attr);
	}
}

/*
 * memberof_get_config()
 *
 * Returns a pointer to the main config.  You should call
 * memberof_rlock_config() first so the main config doesn't
 * get modified out from under you.
 */
MemberOfConfig *
memberof_get_config()
{
	return &theConfig;
}

/*
 * memberof_rlock_config()
 *
 * Gets a non-exclusive lock on the main config.  This will
 * prevent the config from being changed out from under you
 * while you read it, but it will still allow other threads
 * to read the config at the same time.
 */
void
memberof_rlock_config()
{
	slapi_rwlock_rdlock(memberof_config_lock);
}

/*
 * memberof_wlock_config()
 * 
 * Gets an exclusive lock on the main config.  This should
 * be called if you need to write to the main config.
 */
void
memberof_wlock_config()
{
	slapi_rwlock_wrlock(memberof_config_lock);
}

/*
 * memberof_unlock_config()
 *
 * Unlocks the main config.
 */
void
memberof_unlock_config()
{
	slapi_rwlock_unlock(memberof_config_lock);
}

int
memberof_config_get_all_backends()
{
	int all_backends;

	slapi_rwlock_rdlock(memberof_config_lock);
	all_backends = theConfig.allBackends;
	slapi_rwlock_unlock(memberof_config_lock);

	return all_backends;
}

Slapi_DN *
memberof_config_get_entry_scope()
{
	Slapi_DN *entry_scope;

	slapi_rwlock_rdlock(memberof_config_lock);
	entry_scope = theConfig.entryScope;
	slapi_rwlock_unlock(memberof_config_lock);

	return entry_scope;
}

Slapi_DN *
memberof_config_get_entry_scope_exclude_subtree()
{
	Slapi_DN *entry_exclude_subtree;

	slapi_rwlock_rdlock(memberof_config_lock);
	entry_exclude_subtree = theConfig.entryScopeExcludeSubtree;
	slapi_rwlock_unlock(memberof_config_lock);

	return entry_exclude_subtree;
}
/*
 * Check if we are modifying the config, or changing the shared config entry
 */
int
memberof_shared_config_validate(Slapi_PBlock *pb)
{
	Slapi_Entry *e = 0;
	Slapi_Entry *resulting_e = 0;
	Slapi_DN *sdn = 0;
	Slapi_Mods *smods = 0;
	LDAPMod **mods = NULL;
	char returntext[SLAPI_DSE_RETURNTEXT_SIZE];
	int ret = SLAPI_PLUGIN_SUCCESS;

	slapi_pblock_get(pb, SLAPI_TARGET_SDN, &sdn);

   	if (slapi_sdn_issuffix(sdn, memberof_get_config_area()) &&
   	    slapi_sdn_compare(sdn, memberof_get_config_area()) == 0)
   	{
   		/*
   		 * This is the shared config entry.  Apply the mods and set/validate
   		 * the config
   		 */
		int result = 0;

		slapi_pblock_get(pb, SLAPI_ENTRY_PRE_OP, &e);
		if(e){
			slapi_pblock_get(pb, SLAPI_MODIFY_MODS, &mods);
			smods = slapi_mods_new();
			slapi_mods_init_byref(smods, mods);

			/* Create a copy of the entry and apply the
			 * mods to create the resulting entry. */
			resulting_e = slapi_entry_dup(e);
			if (mods && (slapi_entry_apply_mods(resulting_e, mods) != LDAP_SUCCESS)) {
				/* we don't care about this, the update is invalid and will be caught later */
				goto bail;
			}

			if ( SLAPI_DSE_CALLBACK_ERROR == memberof_validate_config (pb, NULL, resulting_e, &ret, returntext,0)) {
				slapi_log_error( SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM,
								"%s", returntext);
				ret = LDAP_UNWILLING_TO_PERFORM;
			}
		} else {
			slapi_log_error( SLAPI_LOG_FATAL, MEMBEROF_PLUGIN_SUBSYSTEM, "memberof_shared_config_validate: "
											"Unable to locate shared config entry (%s) error %d\n",
											slapi_sdn_get_dn(memberof_get_config_area()), result);
			ret = LDAP_UNWILLING_TO_PERFORM;
		}
   	}

bail:
	slapi_mods_free(&smods);
	slapi_entry_free(resulting_e);

	return ret;
}
