#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <string>
#include <vector>
#include "ldap++.h"
#include <ldap.h>

namespace ldap_client
{
/**
 * Sets the system-wide LDAP debug level.
 *
 * @param newlevel New debug level (see the LDAP manual).
 */
void LDAPSetDebuglevel(int newlevel)
{
	int rc = ldap_set_option(NULL, LDAP_OPT_DEBUG_LEVEL, &newlevel);
	if (rc)
		LDAPErrCode2Exception(NULL, rc);

	rc = ber_set_option(NULL, LBER_OPT_DEBUG_LEVEL, &newlevel);
	if (rc)
		LDAPErrCode2Exception(NULL, rc);
}

/**
 * Set the LDAP CA certificate file.
 *
 * @param path Path to the CA certificate file the LDAP server certificate
 *             has been signed with.
 */
void LDAPSetCACert(std::string path)
{
	int rc;

	rc = ldap_set_option(NULL, LDAP_OPT_X_TLS_CACERTFILE, path.c_str());
	if (rc)
		LDAPErrCode2Exception(NULL, rc);
}

/**
 * Establish a new LDAP connection to the given URI.
 *
 * @param uri     URI of the LDAP server.
 * @param version LDAP version to use (LDAP_VERSION2 or LDAP_VERSION3).
 */
LDAPConnection::LDAPConnection(std::string uri, int version)
{
	int rc = ldap_initialize(&_ldap, uri.c_str());
	if (rc)
		LDAPErrCode2Exception(_ldap, rc);

	SetVersion(version);
}

/**
 * Disconnect from the LDAP server, also unbind.
 */
LDAPConnection::~LDAPConnection()
{
	ldap_unbind_ext_s(_ldap, 0, 0);
}

/**
 * Retrieve the most recently occurred error message.
 *
 * @return String containing the most recent error message.
 */
std::string LDAPConnection::GetLastError()
{
	char *str;
	int rc;

	rc = ldap_get_option(_ldap, LDAP_OPT_ERROR_STRING, &str);
	if (rc)
		LDAPErrCode2Exception(_ldap, rc);

	return std::string(str);
}

/**
 * Set the LDAP version used in the connection.
 *
 * @param newversion New LDAP version (LDAP_VERSION2 or LDAP_VERSION3).
 * @throws LDAPException Invalid LDAP version or unable to set.
 */
void LDAPConnection::SetVersion(int newversion)
{
	int rc;

	if (newversion != LDAP_VERSION2 && newversion != LDAP_VERSION3)
		throw new LDAPErrParamError("Unsupported LDAP version");

	rc = ldap_set_option(_ldap, LDAP_OPT_PROTOCOL_VERSION, &newversion);
	if (rc)
		LDAPErrCode2Exception(_ldap, rc);
}

/**
 * Perform LDAP authentication using a simple BIND.
 * You should use SASLBind instead.
 *
 * @param user     LDAP user name (typically a DN).
 * @param password LDAP password as a string.
 * @throws LDAPException Unable to perform bind.
 */
void LDAPConnection::SimpleBind(std::string user, std::string password)
{
	struct berval passwd;
	int rc;

	passwd.bv_val = strdup(user.c_str());
	passwd.bv_len = user.length();

	rc = ldap_sasl_bind_s(_ldap, user.c_str(), LDAP_SASL_SIMPLE, &passwd,
			0, 0, 0);

	free(passwd.bv_val);
	if (rc)
		LDAPErrCode2Exception(_ldap, rc);
}

/**
 * Perform SASL bind. Currently, essentially the same as simple bind.
 *
 * @param user     LDAP user name (typically a DN).
 * @param password LDAP password as a string.
 * @throws LDAPException Unable to perform bind.
 */
void LDAPConnection::SASLBind(std::string user, std::string password)
{
	LDAPControl **sctrlsp = NULL, **cctrlsp = NULL;
	struct berval passwd;
	int rc;

	passwd.bv_val = ber_strdup(password.c_str());
	passwd.bv_len = strlen(passwd.bv_val);

	rc = ldap_sasl_bind_s(_ldap, user.c_str(), LDAP_SASL_SIMPLE,
		&passwd, sctrlsp, cctrlsp, NULL);
	if (passwd.bv_val)
		ber_memfree(passwd.bv_val);
	if (rc)
		LDAPErrCode2Exception(_ldap, rc);
}

/**
 * Search for LDAP records matching a given filter.
 *
 * @param base    Search base to start looking from.
 * @param scope   LDAP search scope (e.g. ONE, SUB, etc.)
 * @param filter  Filter string (e.g. attribute=value).
 * @param attrs   Vector of attributes to fetch in the record.
 * @param timeout Number of milliseconds to wait for an answer.
 * @return LDAPResult object containing all LDAP results returned by
 *         the query.
 * @throws LDAPException An error occurred processing the search query.
 */
LDAPResult* LDAPConnection::Search(const std::string base, int scope,
	const std::string filter, const std::vector<std::string> attrs,
	long timeout)
{
	std::vector<char*> attrlist;
	timeval tv;
	LDAPMessage *msg;
	int rc, i;

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = (timeout % 1000) * 1000;

	for (i = 0; i < attrs.size(); i++)
		attrlist.push_back(const_cast<char*>(attrs[i].c_str()));

	attrlist.push_back(0);

	rc = ldap_search_ext_s(_ldap, base.c_str(), scope,
		filter.c_str(), &attrlist[0], 0, 0, 0, &tv, 0, &msg);

	if (rc)
		LDAPErrCode2Exception(_ldap, rc);

	return new LDAPResult(this, msg);
}

/**
 * Search for LDAP records matching a given filter. A default timeout of
 * 30 seconds is applied.
 *
 * @param base    Search base to start looking from.
 * @param scope   LDAP search scope (e.g. ONE, SUB, etc.)
 * @param filter  Filter string (e.g. attribute=value).
 * @param attrs   Vector of attributes to fetch in the record.
 * @return LDAPResult object containing all LDAP results returned by
 *         the query.
 * @throws LDAPException An error occurred processing the search query.
 */
LDAPResult* LDAPConnection::Search(const std::string base, int scope,
	const std::string filter, const std::vector<std::string> attrs)
{
	return Search(base, scope, filter, attrs, 30000);
}

/**
 * Search for LDAP records matching a given filter. All attributes of any
 * matching record are fetched.
 *
 * @param base    Search base to start looking from.
 * @param scope   LDAP search scope (e.g. ONE, SUB, etc.)
 * @param filter  Filter string (e.g. attribute=value).
 * @param timeout Number of milliseconds to wait for a result.
 * @return LDAPResult object containing all LDAP results returned by
 *         the query.
 * @throws LDAPException An error occurred processing the search query.
 */
LDAPResult* LDAPConnection::Search(const std::string base, int scope,
	const std::string filter, long timeout)
{
	return Search(base, scope, filter, kLdapFilterAll, timeout);
}

/**
 * Search for LDAP records matching a given filter. A default timeout of
 * 30 seconds is applied. All attributes of any matching record are fetched.
 *
 * @param base    Search base to start looking from.
 * @param scope   LDAP search scope (e.g. ONE, SUB, etc.)
 * @param filter  Filter string (e.g. attribute=value).
 * @return LDAPResult object containing all LDAP results returned by
 *         the query.
 * @throws LDAPException An error occurred processing the search query.
 */
LDAPResult* LDAPConnection::Search(const std::string base, int scope,
	const std::string filter)
{
	return Search(base, scope, filter, kLdapFilterAll, 30000);
}

/**
 * Search for LDAP records matching a given filter. All attributes of any
 * matching record are fetched. A subtree search is performed.
 *
 * @param base    Search base to start looking from.
 * @param filter  Filter string (e.g. attribute=value).
 * @param timeout Number of milliseconds to wait for a result.
 * @return LDAPResult object containing all LDAP results returned by
 *         the query.
 * @throws LDAPException An error occurred processing the search query.
 */
LDAPResult* LDAPConnection::Search(const std::string base,
	const std::string filter, long timeout)
{
	return Search(base, LDAP_SCOPE_SUBTREE, filter, kLdapFilterAll,
		timeout);
}

/**
 * Search for LDAP records matching a given filter. A default timeout of
 * 30 seconds is applied. All attributes of any matching record are
 * fetched. A subtree search is performed.
 *
 * @param base    Search base to start looking from.
 * @param filter  Filter string (e.g. attribute=value).
 * @return LDAPResult object containing all LDAP results returned by
 *         the query.
 * @throws LDAPException An error occurred processing the search query.
 */
LDAPResult* LDAPConnection::Search(const std::string base,
	const std::string filter)
{
	return Search(base, LDAP_SCOPE_SUBTREE, filter, kLdapFilterAll, 30000);
}
}
