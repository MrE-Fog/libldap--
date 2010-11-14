#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <vector>
#include <ldap++.h>
#include <ldap.h>

namespace ldap_client
{
LDAPConnection::LDAPConnection(std::string uri)
{
	int rc = ldap_initialize(&this->_ldap, uri.c_str());
	if (rc)
		LDAPErrCode2Exception(rc);
}

LDAPConnection::~LDAPConnection()
{
	ldap_unbind(_ldap);
}

void LDAPConnection::SetDebuglevel(int newlevel)
{
	int rc = ldap_set_option(this->_ldap, LDAP_OPT_DEBUG_LEVEL, &newlevel);
	if (rc)
		LDAPErrCode2Exception(rc);
}

void LDAPConnection::SimpleBind(std::string user, std::string password)
{
	int rc = ldap_simple_bind_s(this->_ldap, user.c_str(), password.c_str());
	if (rc)
		LDAPErrCode2Exception(rc);
}

LDAPResult* LDAPConnection::Search(const std::string base, int scope,
	const std::string filter, const std::vector<std::string> attrs,
	long timeout)
{
	char* attrlist[attrs.size() + 1];
	timeval tv;
	LDAPMessage *msg;
	int rc, i;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	for (i = 0; i < attrs.size(); i++)
		attrlist[i] = strndup(attrs[i].c_str(), attrs[i].length());

	attrlist[i] = 0;

	rc = ldap_search_ext_s(_ldap, base.c_str(), scope,
		filter.c_str(), attrlist, 0, 0, 0, &tv, 0, &msg);

	for (i = 0; i < attrs.size(); i++)
		delete attrlist[i];

	if (rc)
		LDAPErrCode2Exception(rc);

	return new LDAPResult(this, msg);
}

LDAPResult* LDAPConnection::Search(const std::string base, int scope,
	const std::string filter, const std::vector<std::string> attrs)
{
	return Search(base, scope, filter, kLdapFilterAll, 30000);
}

LDAPResult* LDAPConnection::Search(const std::string base, int scope,
	const std::string filter, long timeout)
{
	return Search(base, scope, filter, kLdapFilterAll, timeout);
}

LDAPResult* LDAPConnection::Search(const std::string base, int scope,
	const std::string filter)
{
	return Search(base, scope, filter, kLdapFilterAll, 30000);
}

LDAPResult* LDAPConnection::Search(const std::string base,
	const std::string filter, long timeout)
{
	return Search(base, LDAP_SCOPE_SUBTREE, filter, kLdapFilterAll,
		timeout);
}

LDAPResult* LDAPConnection::Search(const std::string base,
	const std::string filter)
{
	return Search(base, LDAP_SCOPE_SUBTREE, filter, kLdapFilterAll, 30000);
}
}
