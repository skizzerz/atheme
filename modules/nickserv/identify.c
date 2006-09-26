/*
 * Copyright (c) 2005-2006 William Pitcock, et al.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the NickServ IDENTIFY and LOGIN functions.
 *
 * $Id: identify.c 6493 2006-09-26 15:50:27Z jilles $
 */

#include "atheme.h"

/* Check whether we are compiling IDENTIFY or LOGIN */
#ifdef NICKSERV_LOGIN
#define COMMAND_UC "LOGIN"
#define COMMAND_LC "login"
#else
#define COMMAND_UC "IDENTIFY"
#define COMMAND_LC "identify"
#endif

DECLARE_MODULE_V1
(
	"nickserv/" COMMAND_LC, FALSE, _modinit, _moddeinit,
	"$Id: identify.c 6493 2006-09-26 15:50:27Z jilles $",
	"Atheme Development Group <http://www.atheme.org>"
);

static void ns_cmd_login(sourceinfo_t *si, int parc, char *parv[]);

#ifdef NICKSERV_LOGIN
command_t ns_login = { "LOGIN", "Authenticates to a services account.", AC_NONE, 2, ns_cmd_login };
#else
command_t ns_identify = { "IDENTIFY", "Identifies to services for a nickname.", AC_NONE, 2, ns_cmd_login };
command_t ns_id = { "ID", "Alias for IDENTIFY", AC_NONE, 2, ns_cmd_login };
#endif

list_t *ns_cmdtree, *ns_helptree, *ms_cmdtree;

void _modinit(module_t *m)
{
	MODULE_USE_SYMBOL(ns_cmdtree, "nickserv/main", "ns_cmdtree");
	MODULE_USE_SYMBOL(ns_helptree, "nickserv/main", "ns_helptree");

#ifdef NICKSERV_LOGIN
	command_add(&ns_login, ns_cmdtree);
	help_addentry(ns_helptree, "LOGIN", "help/nickserv/login", NULL);
#else
	command_add(&ns_identify, ns_cmdtree);
	command_add(&ns_id, ns_cmdtree);
	help_addentry(ns_helptree, "IDENTIFY", "help/nickserv/identify", NULL);
	help_addentry(ns_helptree, "ID", "help/nickserv/identify", NULL);
#endif
}

void _moddeinit()
{
#ifdef NICKSERV_LOGIN
	command_delete(&ns_login, ns_cmdtree);
	help_delentry(ns_helptree, "LOGIN");
#else
	command_delete(&ns_identify, ns_cmdtree);
	command_delete(&ns_id, ns_cmdtree);
	help_delentry(ns_helptree, "IDENTIFY");
	help_delentry(ns_helptree, "ID");
#endif
}

static void ns_cmd_login(sourceinfo_t *si, int parc, char *parv[])
{
	user_t *u = si->su;
	myuser_t *mu;
	chanuser_t *cu;
	chanacs_t *ca;
	node_t *n, *tn;
	char *target = parv[0];
	char *password = parv[1];
	char buf[BUFSIZE], strfbuf[32];
	char lau[BUFSIZE], lao[BUFSIZE];
	struct tm tm;
	metadata_t *md_failnum;

#ifndef NICKSERV_LOGIN
	if (!nicksvs.no_nick_ownership && target && !password)
	{
		password = target;
		target = si->su->nick;
	}
#endif

	if (!target || !password)
	{
		command_fail(si, fault_needmoreparams, STR_INSUFFICIENT_PARAMS, COMMAND_UC);
		command_fail(si, fault_needmoreparams, nicksvs.no_nick_ownership ? "Syntax: " COMMAND_UC " <account> <password>" : "Syntax: " COMMAND_UC " [nick] <password>");
		return;
	}

	mu = myuser_find(target);

	if (!mu)
	{
		command_fail(si, fault_nosuch_target, "\2%s\2 is not a registered nickname.", target);
		return;
	}

	if (metadata_find(mu, METADATA_USER, "private:freeze:freezer"))
	{
		command_fail(si, fault_authfail, nicksvs.no_nick_ownership ? "You cannot login as \2%s\2 because the account has been frozen." : "You cannot identify to \2%s\2 because the nickname has been frozen.", mu->name);
		logcommand(nicksvs.me, u, CMDLOG_LOGIN, "failed " COMMAND_UC " to %s (frozen)", mu->name);
		return;
	}

	if (u->myuser == mu)
	{
		command_fail(si, fault_authfail, "You are already logged in as \2%s\2.", mu->name);
		return;
	}
	else if (u->myuser != NULL && ircd_on_logout(u->nick, u->myuser->name, NULL))
		/* logout killed the user... */
		return;

	/* we use this in both cases, so set it up here. may be NULL. */
	md_failnum = metadata_find(mu, METADATA_USER, "private:loginfail:failnum");

	if (verify_password(mu, password))
	{
		if (LIST_LENGTH(&mu->logins) >= me.maxlogins)
		{
			command_fail(si, fault_toomany, "There are already \2%d\2 sessions logged in to \2%s\2 (maximum allowed: %d).", LIST_LENGTH(&mu->logins), mu->name, me.maxlogins);
			logcommand(nicksvs.me, u, CMDLOG_LOGIN, "failed " COMMAND_UC " to %s (too many logins)", mu->name);
			return;
		}

		/* if they are identified to another account, nuke their session first */
		if (u->myuser)
		{
		        u->myuser->lastlogin = CURRTIME;
		        LIST_FOREACH_SAFE(n, tn, u->myuser->logins.head)
		        {
			        if (n->data == u)
		                {
		                        node_del(n, &u->myuser->logins);
		                        node_free(n);
		                        break;
		                }
		        }
		        u->myuser = NULL;
		}

		if (is_soper(mu))
		{
			snoop("SOPER: \2%s\2 as \2%s\2", u->nick, mu->name);
		}

		myuser_notice(nicksvs.nick, mu, "%s!%s@%s has just authenticated as you (%s)", u->nick, u->user, u->vhost, mu->name);

		u->myuser = mu;
		n = node_create();
		node_add(u, n, &mu->logins);

		/* keep track of login address for users */
		strlcpy(lau, u->user, BUFSIZE);
		strlcat(lau, "@", BUFSIZE);
		strlcat(lau, u->vhost, BUFSIZE);
		metadata_add(mu, METADATA_USER, "private:host:vhost", lau);

		/* and for opers */
		strlcpy(lao, u->user, BUFSIZE);
		strlcat(lao, "@", BUFSIZE);
		strlcat(lao, u->host, BUFSIZE);
		metadata_add(mu, METADATA_USER, "private:host:actual", lao);

		logcommand(nicksvs.me, u, CMDLOG_LOGIN, COMMAND_UC);

		command_success_nodata(si, nicksvs.no_nick_ownership ? "You are now logged in as \2%s\2." : "You are now identified for \2%s\2.", u->myuser->name);

		/* check for failed attempts and let them know */
		if (md_failnum && (atoi(md_failnum->value) > 0))
		{
			metadata_t *md_failtime, *md_failaddr;
			time_t ts;

			tm = *localtime(&mu->lastlogin);
			strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

			command_success_nodata(si, "\2%d\2 failed %s since %s.",
				atoi(md_failnum->value), (atoi(md_failnum->value) == 1) ? "login" : "logins", strfbuf);

			md_failtime = metadata_find(mu, METADATA_USER, "private:loginfail:lastfailtime");
			ts = atol(md_failtime->value);
			md_failaddr = metadata_find(mu, METADATA_USER, "private:loginfail:lastfailaddr");

			tm = *localtime(&ts);
			strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

			command_success_nodata(si, "Last failed attempt from: \2%s\2 on %s.",
				md_failaddr->value, strfbuf);

			metadata_delete(mu, METADATA_USER, "private:loginfail:failnum");	/* md_failnum now invalid */
			metadata_delete(mu, METADATA_USER, "private:loginfail:lastfailtime");
			metadata_delete(mu, METADATA_USER, "private:loginfail:lastfailaddr");
		}

		mu->lastlogin = CURRTIME;

		/* XXX: ircd_on_login supports hostmasking, we just dont have it yet. */
		/* don't allow them to join regonly chans until their
		 * email is verified */
		if (!(mu->flags & MU_WAITAUTH))
			ircd_on_login(si->su->nick, mu->name, NULL);

		hook_call_event("user_identify", u);

		/* now we get to check for xOP */
		/* we don't check for host access yet (could match different
		 * entries because of services cloaks) */
		LIST_FOREACH(n, mu->chanacs.head)
		{
			ca = (chanacs_t *)n->data;

			cu = chanuser_find(ca->mychan->chan, u);
			if (cu && chansvs.me != NULL)
			{
				if (ca->level & CA_AKICK && !(ca->level & CA_REMOVE))
				{
					/* Stay on channel if this would empty it -- jilles */
					if (ca->mychan->chan->nummembers <= (config_options.join_chans ? 2 : 1))
					{
						ca->mychan->flags |= MC_INHABIT;
						if (!config_options.join_chans)
							join(cu->chan->name, chansvs.nick);
					}
					ban(chansvs.me->me, ca->mychan->chan, u);
					remove_ban_exceptions(chansvs.me->me, ca->mychan->chan, u);
					kick(chansvs.nick, ca->mychan->name, u->nick, "User is banned from this channel");
					continue;
				}

				if (ca->level & CA_USEDUPDATE)
					ca->mychan->used = CURRTIME;

				if (ca->mychan->flags & MC_NOOP || mu->flags & MU_NOOP)
					continue;

				if (ircd->uses_owner && !(cu->modes & ircd->owner_mode) && should_owner(ca->mychan, ca->myuser))
				{
					modestack_mode_param(chansvs.nick, ca->mychan->name, MTYPE_ADD, ircd->owner_mchar[1], CLIENT_NAME(u));
					cu->modes |= ircd->owner_mode;
				}

				if (ircd->uses_protect && !(cu->modes & ircd->protect_mode) && should_protect(ca->mychan, ca->myuser))
				{
					modestack_mode_param(chansvs.nick, ca->mychan->name, MTYPE_ADD, ircd->protect_mchar[1], CLIENT_NAME(u));
					cu->modes |= ircd->protect_mode;
				}

				if (!(cu->modes & CMODE_OP) && ca->level & CA_AUTOOP)
				{
					modestack_mode_param(chansvs.nick, ca->mychan->name, MTYPE_ADD, 'o', CLIENT_NAME(u));
					cu->modes |= CMODE_OP;
				}

				if (ircd->uses_halfops && !(cu->modes & (CMODE_OP | ircd->halfops_mode)) && ca->level & CA_AUTOHALFOP)
				{
					modestack_mode_param(chansvs.nick, ca->mychan->name, MTYPE_ADD, 'h', CLIENT_NAME(u));
					cu->modes |= ircd->halfops_mode;
				}

				if (!(cu->modes & (CMODE_OP | ircd->halfops_mode | CMODE_VOICE)) && ca->level & CA_AUTOVOICE)
				{
					modestack_mode_param(chansvs.nick, ca->mychan->name, MTYPE_ADD, 'v', CLIENT_NAME(u));
					cu->modes |= CMODE_VOICE;
				}
			}
		}

		return;
	}

	logcommand(nicksvs.me, u, CMDLOG_LOGIN, "failed " COMMAND_UC " to %s (bad password)", mu->name);

	command_fail(si, fault_authfail, "Invalid password for \2%s\2.", mu->name);

	/* record the failed attempts */
	/* note that we reuse this buffer later when warning opers about failed logins */
	snprintf(buf, sizeof buf, "%s!%s@%s", u->nick, u->user, u->vhost);

	/* increment fail count */
	if (md_failnum && (atoi(md_failnum->value) > 0))
		md_failnum = metadata_add(mu, METADATA_USER, "private:loginfail:failnum",
								itoa(atoi(md_failnum->value) + 1));
	else
		md_failnum = metadata_add(mu, METADATA_USER, "private:loginfail:failnum", "1");
	metadata_add(mu, METADATA_USER, "private:loginfail:lastfailaddr", buf);
	metadata_add(mu, METADATA_USER, "private:loginfail:lastfailtime", itoa(CURRTIME));

	if (atoi(md_failnum->value) == 10)
	{
		time_t ts = CURRTIME;
		tm = *localtime(&ts);
		strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

		wallops("Warning: Numerous failed login attempts to \2%s\2. Last attempt received from \2%s\2 on %s.", mu->name, buf, strfbuf);
	}
}
