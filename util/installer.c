/*
  Copyright (C) 2007 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  There are special exceptions to the terms and conditions of the GPL
  as it is applied to this software. View the full text of the exception
  in file LICENSE.exceptions in the top-level directory of this software
  distribution.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Installer wrapper implementations.
 *
 * How to add a data-source option:
 *    Search for DS_PARAM, and follow the code around it
 *    Add an extra field to the DataSource struct (installer.h)
 *    Add a default value in ds_new()
 *    Initialize/destroy them in ds_new/ds_delete
 *    
 */
#include "stringutil.h"
#include "installer.h"

/* ODBC Installer Config Wrapper */

/* a few constants */
static SQLWCHAR W_EMPTY[]= {0};
static SQLWCHAR W_ODBCINST_INI[]=
  {'O', 'D', 'B', 'C', 'I', 'N', 'S', 'T', '.', 'I', 'N', 'I', 0};
static SQLWCHAR W_ODBC_INI[]= {'O', 'D', 'B', 'C', '.', 'I', 'N', 'I', 0};

static SQLWCHAR W_DSN[]= {'D', 'S', 'N', 0};
static SQLWCHAR W_DRIVER[]= {'D', 'R', 'I', 'V', 'E', 'R', 0};
static SQLWCHAR W_DESCRIPTION[]=
  {'D', 'E', 'S', 'C', 'R', 'I', 'P', 'T', 'I', 'O', 'N', 0};
static SQLWCHAR W_SERVER[]= {'S', 'E', 'R', 'V', 'E', 'R', 0};
static SQLWCHAR W_UID[]= {'U', 'I', 'D', 0};
static SQLWCHAR W_USER[]= {'U', 'S', 'E', 'R', 0};
static SQLWCHAR W_PWD[]= {'P', 'W', 'D', 0};
static SQLWCHAR W_PASSWORD[]= {'P', 'A', 'S', 'S', 'W', 'O', 'R', 'D', 0};
static SQLWCHAR W_DB[]= {'D', 'B', 0};
static SQLWCHAR W_DATABASE[]= {'D', 'A', 'T', 'A', 'B', 'A', 'S', 'E', 0};
static SQLWCHAR W_SOCKET[]= {'S', 'O', 'C', 'K', 'E', 'T', 0};
static SQLWCHAR W_INITSTMT[]= {'I', 'N', 'I', 'T', 'S', 'T', 'M', 'T', 0};
static SQLWCHAR W_OPTION[]= {'O', 'P', 'T', 'I', 'O', 'N', 0};
static SQLWCHAR W_CHARSET[]= {'C', 'H', 'A', 'R', 'S', 'E', 'T', 0};
static SQLWCHAR W_SSLKEY[]= {'S', 'S', 'L', 'K', 'E', 'Y', 0};
static SQLWCHAR W_SSLCERT[]= {'S', 'S', 'L', 'C', 'E', 'R', 'T', 0};
static SQLWCHAR W_SSLCA[]= {'S', 'S', 'L', 'C', 'A', 0};
static SQLWCHAR W_SSLCAPATH[]=
  {'S', 'S', 'L', 'C', 'A', 'P', 'A', 'T', 'H', 0};
static SQLWCHAR W_SSLCIPHER[]=
  {'S', 'S', 'L', 'C', 'I', 'P', 'H', 'E', 'R', 0};
static SQLWCHAR W_PORT[]= {'P', 'O', 'R', 'T', 0};
static SQLWCHAR W_SETUP[]= {'S', 'E', 'T', 'U', 'P', 0};
/* DS_PARAM */

/* convenience macro to append a single character */
#define APPEND_SQLWCHAR(buf, ctr, c) {\
  if (ctr) { \
    *((buf)++)= (c); \
    if (--(ctr)) \
        *(buf)= 0; \
  } \
}


/*
 * Check whether a parameter value needs escaping.
 */
static int value_needs_escaped(SQLWCHAR *str)
{
  SQLWCHAR c;
  while (str && (c= *str++))
  {
    if (c >= '0' && c <= '9')
      continue;
    else if (c >= 'a' && c <= 'z')
      continue;
    else if (c >= 'A' && c <= 'Z')
      continue;
    /* other non-alphanumeric characters that don't need escaping */
    switch (c)
    {
    case '_':
    case ' ':
    case '.':
      continue;
    }
    return 1;
  }
  return 0;
}


/*
 * Convenience function to get the current config mode.
 */
UWORD config_get()
{
  UWORD mode;
  SQLGetConfigMode(&mode);
  return mode;
}


/*
 * Convenience function to set the current config mode. Returns the
 * mode in use when the function was called.
 */
UWORD config_set(UWORD mode)
{
  UWORD current= config_get();
  SQLSetConfigMode(mode);
  return current;
}


/* ODBC Installer Driver Wrapper */


/*
 * Create a new driver object. All string data is pre-allocated.
 */
Driver *driver_new()
{
  Driver *driver= (Driver *)my_malloc(sizeof(Driver), MYF(0));
  if (!driver)
    return NULL;
  driver->name= (SQLWCHAR *)my_malloc(ODBCDRIVER_STRLEN * sizeof(SQLWCHAR),
                                      MYF(0));
  if (!driver->name)
  {
    x_free(driver);
    return NULL;
  }
  driver->lib= (SQLWCHAR *)my_malloc(ODBCDRIVER_STRLEN * sizeof(SQLWCHAR),
                                     MYF(0));
  if (!driver->lib)
  {
    x_free(driver);
    x_free(driver->name);
    return NULL;
  }
  driver->setup_lib= (SQLWCHAR *)my_malloc(ODBCDRIVER_STRLEN *
                                           sizeof(SQLWCHAR), MYF(0));
  if (!driver->setup_lib)
  {
    x_free(driver);
    x_free(driver->name);
    x_free(driver->lib);
    return NULL;
  }
  /* init to empty strings */
  driver->name[0]= 0;
  driver->lib[0]= 0;
  driver->setup_lib[0]= 0;
  driver->name8= NULL;
  driver->lib8= NULL;
  driver->setup_lib8= NULL;
  return driver;
}


/*
 * Delete an existing driver object.
 */
void driver_delete(Driver *driver)
{
  x_free(driver->name);
  x_free(driver->lib);
  x_free(driver->setup_lib);
  x_free(driver->name8);
  x_free(driver->lib8);
  x_free(driver->setup_lib8);
  x_free(driver);
}


/*
 * Lookup a driver in the system. The driver name is read from the given
 * object. If greater-than zero is returned, additional information
 * can be obtained from SQLInstallerError(). A less-than zero return code
 * indicates that the driver could not be found.
 */
int driver_lookup(Driver *driver)
{
  SQLWCHAR buf[4096];
  SQLWCHAR *entries= buf;
  SQLWCHAR *dest;

  /* get entries and make sure the driver exists */
  if (SQLGetPrivateProfileStringW(driver->name, NULL, W_EMPTY, buf, 4096,
                                  W_ODBCINST_INI) < 1)
    return -1;

  /* read the needed driver attributes */
  while (*entries)
  {
    dest= NULL;
    if (!sqlwcharcasecmp(W_DRIVER, entries))
      dest= driver->lib;
    else if (!sqlwcharcasecmp(W_SETUP, entries))
      dest= driver->setup_lib;
    else
      { /* unknown/unused entry */ }

    /* get the value if it's one we're looking for */
    if (dest && SQLGetPrivateProfileStringW(driver->name, entries, W_EMPTY,
                                            dest, ODBCDRIVER_STRLEN,
                                            W_ODBCINST_INI) < 1)
      return 1;

    entries += sqlwcharlen(entries) + 1;
  }

  return 0;
}


/*
 * Read the semi-colon delimited key-value pairs the attributes
 * necessary to popular the driver object.
 */
int driver_from_kvpair_semicolon(Driver *driver, const SQLWCHAR *attrs)
{
  const SQLWCHAR *split;
  const SQLWCHAR *end;
  SQLWCHAR attribute[100];
  SQLWCHAR *dest;

  while (*attrs)
  {
    dest= NULL;

    /* invalid key-value pair if no equals */
    if ((split= sqlwcharchr(attrs, '=')) == NULL)
      return 1;

    /* get end of key-value pair */
    if ((end= sqlwcharchr(attrs, ';')) == NULL)
      end= attrs + sqlwcharlen(attrs);

    /* pull out the attribute name */
    memcpy(attribute, attrs, (split - attrs) * sizeof(SQLWCHAR));
    attribute[split - attrs]= 0; /* add null term */
    split++;

    /* if its one we want, copy it over */
    if (!sqlwcharcasecmp(W_DRIVER, attribute))
      dest= driver->lib;
    else if (!sqlwcharcasecmp(W_SETUP, attribute))
      dest= driver->setup_lib;
    else
      { /* unknown/unused attribute */ }

    if (dest)
    {
      memcpy(dest, split, (end - split) * sizeof(SQLWCHAR));
      dest[end - split]= 0; /* add null term */
    }

    /* advanced to next attribute */
    attrs= end;
    if (*end)
      attrs++;
  }

  return 0;
}


/*
 * Write the attributes of the driver object into key-value pairs
 * separated by single NULL chars.
 */
int driver_to_kvpair_null(Driver *driver, SQLWCHAR *attrs, size_t attrslen)
{
  attrs+= sqlwcharncat2(attrs, driver->name, &attrslen);

  /* append NULL-separator */
  *(attrs++)= 0;
  attrslen--;

  attrs+= sqlwcharncat2(attrs, W_DRIVER, &attrslen);
  APPEND_SQLWCHAR(attrs, attrslen, '=');
  attrs+= sqlwcharncat2(attrs, driver->lib, &attrslen);

  /* append NULL-separator */
  *(attrs++)= 0;
  attrslen--;

  if (*driver->setup_lib)
  {
    attrs+= sqlwcharncat2(attrs, W_SETUP, &attrslen);
    APPEND_SQLWCHAR(attrs, attrslen, '=');
    attrs+= sqlwcharncat2(attrs, driver->setup_lib, &attrslen);

    /* append NULL-separator */
    *(attrs++)= 0;
    attrslen--;
  }
  if (attrslen--)
    *attrs= 0;
  return !(attrslen > 0);
}


/* ODBC Installer Data Source Wrapper */


/*
 * Create a new data source object.
 */
DataSource *ds_new()
{
  DataSource *ds= (DataSource *)my_malloc(sizeof(DataSource), MYF(0));
  if (!ds)
    return NULL;
  memset(ds, 0, sizeof(DataSource));

  ds->port= 3306;

  return ds;
}


/*
 * Delete an existing data source object.
 */
void ds_delete(DataSource *ds)
{
  x_free(ds->name);
  x_free(ds->driver);
  x_free(ds->description);
  x_free(ds->server);
  x_free(ds->uid);
  x_free(ds->pwd);
  x_free(ds->database);
  x_free(ds->socket);
  x_free(ds->initstmt);
  x_free(ds->option);
  x_free(ds->charset);
  x_free(ds->sslkey);
  x_free(ds->sslcert);
  x_free(ds->sslca);
  x_free(ds->sslcapath);
  x_free(ds->sslcipher);
  
  x_free(ds->name8);
  x_free(ds->driver8);
  x_free(ds->description8);
  x_free(ds->server8);
  x_free(ds->uid8);
  x_free(ds->pwd8);
  x_free(ds->database8);
  x_free(ds->socket8);
  x_free(ds->initstmt8);
  x_free(ds->charset8);
  x_free(ds->sslkey8);
  x_free(ds->sslcert8);
  x_free(ds->sslca8);
  x_free(ds->sslcapath8);
  x_free(ds->sslcipher8);

  x_free(ds);
}


/*
 * Set a string attribute of a given data source object. The string
 * will be copied into the object.
 */
int ds_set_strattr(SQLWCHAR **attr, const SQLWCHAR *val)
{
  if (*attr)
    x_free(*attr);
  if (val && *val)
    *attr= sqlwchardup(val, SQL_NTS);
  else
    *attr= NULL;
  return *attr || 0;
}


/*
 * Same as ds_set_strattr, but allows truncating the given string. If
 * charcount is 0 or SQL_NTS, it will act the same as ds_set_strattr.
 */
int ds_set_strnattr(SQLWCHAR **attr, const SQLWCHAR *val, size_t charcount)
{
  if (*attr)
    x_free(*attr);

  if (charcount == SQL_NTS)
    charcount= sqlwcharlen(val);

  if (!charcount)
  {
    *attr= NULL;
    return 1;
  }

  if (val && *val)
    *attr= sqlwchardup(val, charcount);
  else
    *attr= NULL;
  return *attr || 0;
}


/*
 * Internal function to map a parameter name of the data source object
 * to the pointer needed to set the parameter. Only one of strdest or
 * intdest will be set. strdest and intdest will be set for populating
 * string (SQLWCHAR *) or int parameters.
 */
void ds_map_param(DataSource *ds, const SQLWCHAR *param,
    SQLWCHAR ***strdest, int **intdest)
{
  *strdest= NULL;
  *intdest= NULL;
  /* parameter aliases can be used here, see W_UID, W_USER */
  if (!sqlwcharcasecmp(W_DSN, param))
    *strdest= &ds->name;
  else if (!sqlwcharcasecmp(W_DRIVER, param))
    *strdest= &ds->driver;
  else if (!sqlwcharcasecmp(W_DESCRIPTION, param))
    *strdest= &ds->description;
  else if (!sqlwcharcasecmp(W_SERVER, param))
    *strdest= &ds->server;
  else if (!sqlwcharcasecmp(W_UID, param))
    *strdest= &ds->uid;
  else if (!sqlwcharcasecmp(W_USER, param))
    *strdest= &ds->uid;
  else if (!sqlwcharcasecmp(W_PWD, param))
    *strdest= &ds->pwd;
  else if (!sqlwcharcasecmp(W_PASSWORD, param))
    *strdest= &ds->pwd;
  else if (!sqlwcharcasecmp(W_DB, param))
    *strdest= &ds->database;
  else if (!sqlwcharcasecmp(W_DATABASE, param))
    *strdest= &ds->database;
  else if (!sqlwcharcasecmp(W_SOCKET, param))
    *strdest= &ds->socket;
  else if (!sqlwcharcasecmp(W_INITSTMT, param))
    *strdest= &ds->initstmt;
  else if (!sqlwcharcasecmp(W_OPTION, param))
    *strdest= &ds->option;
  else if (!sqlwcharcasecmp(W_CHARSET, param))
    *strdest= &ds->charset;
  else if (!sqlwcharcasecmp(W_SSLKEY, param))
    *strdest= &ds->sslkey;
  else if (!sqlwcharcasecmp(W_SSLCERT, param))
    *strdest= &ds->sslcert;
  else if (!sqlwcharcasecmp(W_SSLCA, param))
    *strdest= &ds->sslca;
  else if (!sqlwcharcasecmp(W_SSLCAPATH, param))
    *strdest= &ds->sslcapath;
  else if (!sqlwcharcasecmp(W_SSLCIPHER, param))
    *strdest= &ds->sslcipher;

  else if (!sqlwcharcasecmp(W_PORT, param))
    *intdest= &ds->port;
  /* DS_PARAM */
}


/*
 * Lookup a data source in the system. The name will be read from
 * the object and the rest of the details will be populated.
 *
 * If greater-than zero is returned, additional information
 * can be obtained from SQLInstallerError(). A less-than zero return code
 * indicates that the driver could not be found.
 */
int ds_lookup(DataSource *ds)
{
  SQLWCHAR buf[8192];
  SQLWCHAR *entries= buf;
  SQLWCHAR **dest;
  SQLWCHAR val[256];
  int size;
  int rc= 0;
  UWORD config_mode= config_get();
  int *intdest;

  /* get entries and check if data source exists */
  if ((size= SQLGetPrivateProfileStringW(ds->name, NULL, W_EMPTY,
                                         buf, 8192, W_ODBC_INI)) < 1)
  {
    rc= -1;
    goto end;
  }

  /* Debug code to print the entries returned, char by char */
#ifdef DEBUG_MYODBC_DS_LOOKUP
  {
  int i;
  for (i= 0; i < size; ++i)
    printf("%wc - 0x%x\n", entries[i], entries[i]);
  }
#endif

#ifdef _WIN32
  /*
   * In Windows XP, there is a bug in SQLGetPrivateProfileString
   * when mode is ODBC_BOTH_DSN and we are looking for a system
   * DSN. In this case SQLGetPrivateProfileString will find the
   * system dsn but return a corrupt list of attributes. 
   *
   * See debug code above to print the exact data returned.
   */
  if (config_mode == ODBC_BOTH_DSN &&
      /* two null chars or a null and some bogus character */
      *(entries + sqlwcharlen(entries)) == 0 &&
      (*(entries + sqlwcharlen(entries) + 1) > 0x7f ||
       *(entries + sqlwcharlen(entries) + 1) == 0))
  {
    /* revert to system mode and try again */
    config_set(ODBC_SYSTEM_DSN);
    SQLGetPrivateProfileStringW(ds->name, NULL, W_EMPTY,
                                buf, 8192, W_ODBC_INI);
  }
#endif

  while (*entries)
  {
    ds_map_param(ds, entries, &dest, &intdest);

    if ((dest || intdest) &&
      (size= SQLGetPrivateProfileStringW(ds->name, entries, W_EMPTY,
                                         val, ODBCDATASOURCE_STRLEN,
                                         W_ODBC_INI)) < 0)
    {
      rc= 1;
      goto end;
    }
    else if (!size)
      /* skip blanks */;
    else if (dest)
      ds_set_strattr(dest, val);
    else if (intdest)
      *intdest= sqlwchartoul(val);

    entries += sqlwcharlen(entries) + 1;
  }

end:
  config_set(config_mode);
  return rc;
}


/*
 * Read an attribute list (key/value pairs) into a data source
 * object. Delimiter should probably be 0 or ';'.
 */
int ds_from_kvpair(DataSource *ds, const SQLWCHAR *attrs, SQLWCHAR delim)
{
  const SQLWCHAR *split;
  const SQLWCHAR *end;
  SQLWCHAR **dest;
  SQLWCHAR attribute[100];
  int *intdest;

  while (*attrs)
  {
    if ((split= sqlwcharchr(attrs, (SQLWCHAR)'=')) == NULL)
      return 1;

    if ((end= sqlwcharchr(attrs, delim)) == NULL)
      end= attrs + sqlwcharlen(attrs);

    memcpy(attribute, attrs, (split - attrs) * sizeof(SQLWCHAR));
    attribute[split - attrs]= 0;
    split++;

    ds_map_param(ds, attribute, &dest, &intdest);

    if (dest)
    {
      if (*split == L'{' && *(end - 1) == '}')
        ds_set_strnattr(dest, split + 1, end - split - 2);
      else
        ds_set_strnattr(dest, split, end - split);
    }
    else if (intdest)
    {
      /* we know we have a ; or NULL at the end so we just let it go */
      *intdest= sqlwchartoul(split);
    }

    attrs= end;
    if (*end)
      attrs++;
  }

  return 0;
}


/*
 * Copy data source details into an attribute string. Use attrslen
 * to limit the number of characters placed into the string.
 *
 * Return -1 for an error or truncation, otherwise the number of
 * characters written.
 */
int ds_to_kvpair(DataSource *ds, SQLWCHAR *attrs, size_t attrslen,
         SQLWCHAR delim)
{
  /* TODO centralize all this */
  const SQLWCHAR *params[]= {W_DSN, W_DRIVER, W_DESCRIPTION, W_SERVER,
                             W_UID, W_PWD, W_DATABASE, W_SOCKET, W_INITSTMT,
                             W_PORT, W_OPTION, W_CHARSET, W_SSLKEY,
                             W_SSLCERT, W_SSLCA, W_SSLCAPATH, W_SSLCIPHER};
  /* DS_PARAM */
  int paramcnt= sizeof(params) / sizeof(SQLWCHAR *);
  int i;
  SQLWCHAR **strval;
  int *intval;
  int origchars= attrslen;
  SQLWCHAR numbuf[21];

  for (i= 0; i < paramcnt; ++i)
  {
    ds_map_param(ds, params[i], &strval, &intval);

    /* We skip the driver is dsn is given */
    if (!sqlwcharcasecmp(W_DRIVER, params[i]) && ds->name && *ds->name)
      continue;

    if (strval && *strval && **strval)
    {
      attrs+= sqlwcharncat2(attrs, params[i], &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, '=');
      if (value_needs_escaped(*strval))
      {
        APPEND_SQLWCHAR(attrs, attrslen, '{');
        attrs+= sqlwcharncat2(attrs, *strval, &attrslen);
        APPEND_SQLWCHAR(attrs, attrslen, '}');
      }
      else
        attrs+= sqlwcharncat2(attrs, *strval, &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, delim);
    }
    else if (intval)
    {
      attrs+= sqlwcharncat2(attrs, params[i], &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, '=');
      sqlwcharfromul(numbuf, *intval);
      attrs+= sqlwcharncat2(attrs, numbuf, &attrslen);
      APPEND_SQLWCHAR(attrs, attrslen, delim);
    }

    if (!attrslen)
      /* we don't have enough room */
      return -1;
  }

  /* always ends in delimiter, so overwrite it */
  *(attrs - 1)= 0;

  return origchars - attrslen;
}


/*
 * Utility method for ds_add() to add a single string
 * property via the installer api.
 */
int ds_add_strprop(const SQLWCHAR *name, const SQLWCHAR *propname,
           const SQLWCHAR *propval)
{
  /* don't write if its null or empty string */
  if (propval && *propval)
    return !SQLWritePrivateProfileStringW(name, propname, propval,
                                          W_ODBC_INI);
  return 0;
}


/*
 * Utility method for ds_add() to add a single integer
 * property via the installer api.
 */
int ds_add_intprop(const SQLWCHAR *name, const SQLWCHAR *propname, int propval)
{
  SQLWCHAR buf[21];
  sqlwcharfromul(buf, propval);
  return ds_add_strprop(name, propname, buf);
}


/*
 * Add the given datasource to system. Call SQLInstallerError() to get
 * further error details if non-zero is returned.
 */
int ds_add(DataSource *ds)
{
  /* Validate data source name */
  if (!SQLValidDSNW(ds->name))
    return 1;

  /* remove if exists, FYI SQLRemoveDSNFromIni returns true
   * even if the dsn isnt found, false only if there is a failure */
  if (!SQLRemoveDSNFromIniW(ds->name))
    return 1;

  /* "Create" section for data source */
  if (!SQLWriteDSNToIniW(ds->name, ds->driver))
    return 1;

  /* write all fields (util method takes care of skipping blank fields) */
  if (ds_add_strprop(ds->name, W_DRIVER     , ds->driver     )) goto error;
  if (ds_add_strprop(ds->name, W_DESCRIPTION, ds->description)) goto error;
  if (ds_add_strprop(ds->name, W_SERVER     , ds->server     )) goto error;
  if (ds_add_strprop(ds->name, W_UID        , ds->uid        )) goto error;
  if (ds_add_strprop(ds->name, W_PWD        , ds->pwd        )) goto error;
  if (ds_add_strprop(ds->name, W_DATABASE   , ds->database   )) goto error;
  if (ds_add_strprop(ds->name, W_SOCKET     , ds->socket     )) goto error;
  if (ds_add_strprop(ds->name, W_INITSTMT   , ds->initstmt   )) goto error;
  if (ds_add_strprop(ds->name, W_OPTION     , ds->option     )) goto error;
  if (ds_add_strprop(ds->name, W_CHARSET    , ds->charset    )) goto error;
  if (ds_add_strprop(ds->name, W_SSLKEY     , ds->sslkey     )) goto error;
  if (ds_add_strprop(ds->name, W_SSLCERT    , ds->sslcert    )) goto error;
  if (ds_add_strprop(ds->name, W_SSLCA      , ds->sslca      )) goto error;
  if (ds_add_strprop(ds->name, W_SSLCAPATH  , ds->sslcapath  )) goto error;
  if (ds_add_strprop(ds->name, W_SSLCIPHER  , ds->sslcipher  )) goto error;

  if (ds_add_intprop(ds->name, W_PORT       , ds->port       )) goto error;

  /* DS_PARAM */

  return 0;

error:
  return 1;
}


/*
 * Convenience method to check if data source exists. Set the dsn
 * scope before calling this to narrow the check.
 */
int ds_exists(SQLWCHAR *name)
{
  SQLWCHAR buf[100];

  /* get entries and check if data source exists */
  if (SQLGetPrivateProfileStringW(name, NULL, W_EMPTY, buf, 100, W_ODBC_INI))
    return 0;

  return 1;
}


/*
 * Get a copy of an attribute in UTF-8. You can use the attr8 style
 * pointer and it will be freed when the data source is deleted.
 *
 * ex. SQLCHAR *username= ds_get_utf8attr(ds->uid, &ds->uid8);
 */
SQLCHAR *ds_get_utf8attr(SQLWCHAR *attrw, SQLCHAR **attr8)
{
  SQLINTEGER len= SQL_NTS;
  if (*attr8)
    x_free(*attr8);
  *attr8= sqlwchar_as_utf8(attrw, &len);
  return *attr8;
}
