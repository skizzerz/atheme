/*
 * Copyright (c) 2005 William Pitcock, et al.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the CService SENDPASS function.
 */

#include "atheme.h"

static void ns_cmd_sendpass(sourceinfo_t *si, int parc, char *parv[]);

command_t ns_sendpass = { "SENDPASS", N_("Email registration passwords."), AC_NONE, 2, ns_cmd_sendpass, { .path = "nickserv/sendpass_user" } };

static void
mod_init(module_t *const restrict m)
{
	MODULE_CONFLICT(m, "nickserv/sendpass")
	MODULE_TRY_REQUEST_DEPENDENCY(m, "nickserv/setpass");

	service_named_bind_command("nickserv", &ns_sendpass);
}

static void
mod_deinit(const module_unload_intent_t intent)
{
	service_named_unbind_command("nickserv", &ns_sendpass);
}

enum specialoperation
{
	op_none,
	op_force,
	op_clear
};

static void ns_cmd_sendpass(sourceinfo_t *si, int parc, char *parv[])
{
	myuser_t *mu;
	char *name = parv[0];
	char *key;
	enum specialoperation op = op_none;
	bool ismarked = false;

	if (!name)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, "SENDPASS");
		command_fail(si, fault_needmoreparams, _("Syntax: SENDPASS <account>"));
		return;
	}

	if (parc > 1)
	{
		if (!has_priv(si, PRIV_USER_SENDPASS))
		{
			command_fail(si, fault_noprivs, _("You are not authorized to perform this operation."));
			return;
		}
		else if (!strcasecmp(parv[1], "FORCE"))
			op = op_force;
		else if (!strcasecmp(parv[1], "CLEAR"))
			op = op_clear;
		else
		{
			command_fail(si, fault_badparams, STR_INVALID_PARAMS, "SENDPASS");
			command_fail(si, fault_badparams, _("Syntax: SENDPASS <account> [FORCE|CLEAR]"));
			return;
		}
	}

	if (!(mu = myuser_find_by_nick(name)))
	{
		command_fail(si, fault_nosuch_target, _("\2%s\2 is not registered."), name);
		return;
	}

	if (mu->flags & MU_WAITAUTH)
	{
		command_fail(si, fault_badparams, _("\2%s\2 is not verified."), entity(mu)->name);
		return;
	}

	if (metadata_find(mu, "private:mark:setter"))
	{
		ismarked = true;
		/* don't want to disclose this, so just go ahead... */
	}

	if (op == op_clear)
	{
		if (metadata_find(mu, "private:setpass:key"))
		{
			logcommand(si, CMDLOG_ADMIN, "SENDPASS:CLEAR: \2%s\2", entity(mu)->name);
			metadata_delete(mu, "private:setpass:key");
			metadata_delete(mu, "private:sendpass:sender");
			metadata_delete(mu, "private:sendpass:timestamp");
			command_success_nodata(si, _("The password change key for \2%s\2 has been cleared."), entity(mu)->name);
		}
		else
			command_fail(si, fault_nochange, _("\2%s\2 did not have a password change key outstanding."), entity(mu)->name);
		return;
	}

	if (MOWGLI_LIST_LENGTH(&mu->logins) > 0)
	{
		if (si->smu == mu)
			command_fail(si, fault_already_authed, _("You are logged in and can change your password using the SET PASSWORD command."));
		else
			command_fail(si, fault_noprivs, _("This operation cannot be performed on %s, because someone is logged in to it."), entity(mu)->name);
		return;
	}

	if (metadata_find(mu, "private:freeze:freezer"))
	{
		command_fail(si, fault_noprivs, _("%s has been frozen by the %s administration."), entity(mu)->name, me.netname);
		return;
	}

	if (metadata_find(mu, "private:setpass:key"))
	{
		command_fail(si, fault_alreadyexists, _("\2%s\2 already has a password change key outstanding."), entity(mu)->name);
		if (has_priv(si, PRIV_USER_SENDPASS))
			command_fail(si, fault_alreadyexists, _("Use SENDPASS %s CLEAR to clear it so that a new one can be sent."), entity(mu)->name);
		return;
	}

	key = random_string(12);

	const char *const hash = crypt_string(key, NULL);

	if (!hash)
	{
		command_fail(si, fault_internalerror, _("Hash generation for password change key failed."));
		free(key);
		return;
	}
	if (sendemail(si->su != NULL ? si->su : si->service->me, mu, EMAIL_SETPASS, mu->email, key))
	{
		if (ismarked)
			wallops("%s sent the password for the \2MARKED\2 account %s.", get_oper_name(si), entity(mu)->name);
		logcommand(si, CMDLOG_ADMIN, "SENDPASS: \2%s\2 (change key)", name);
		metadata_add(mu, "private:sendpass:sender", get_oper_name(si));
		metadata_add(mu, "private:sendpass:timestamp", number_to_string(time(NULL)));
		metadata_add(mu, "private:setpass:key", hash);
		command_success_nodata(si, _("The password change key for \2%s\2 has been sent to the corresponding email address."), entity(mu)->name);
	}
	else
		command_fail(si, fault_emailfail, _("Email send failed."));
	free(key);
}

SIMPLE_DECLARE_MODULE_V1("nickserv/sendpass_user", MODULE_UNLOAD_CAPABILITY_OK)
