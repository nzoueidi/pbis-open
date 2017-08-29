/*
 * Copyright (c) BeyondTrust Software.  All rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the license, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.  You should have received a copy
 * of the GNU Lesser General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * LESSER GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

/*
 * Module Name:
 *
 *        gen_keytab.c
 *
 * Abstract:
 *
 *        
 *
 * Authors: Author: rali
 * 
 * Created on: Oct 13, 2016
 *
 */

#include "includes.h"
#include <libgen.h>

static 
DWORD CreateUserKeytabEntry(AppContextTP appContext, PSTR pszDN, PSTR pszName, PSTR pszPass, PSTR pszKeytab);

static
DWORD CreateComputerKeytabEntry(AppContextTP appContext, PSTR pszDn, PSTR pszName, PSTR pszPassword, 
                                PSTR pszSamAccountName, PSTR pszDnsHostName, PSTR pszKeytab,
                                PSTR pszServicePrincipalNameList);

static
DWORD GetDomainDcName(AppContextTP appContext, PSTR pszDomainFromDN, PSTR *pszDcName);


//****************************************************************************************
//
//****************************************************************************************
DWORD ModifyUserKeytabFile(AdtActionTP action, PSTR pszUserName)
{
   DWORD dwError = 0;
   AppContextTP appContext = (AppContextTP) ((AdtActionBaseTP) action)->opaque;

   dwError = CreateUserKeytabEntry(appContext,
                                   action->resetUserPassword.name, 
                                   pszUserName, 
                                   action->resetUserPassword.password,
                                   action->resetUserPassword.keytab);
   ADT_BAIL_ON_ERROR(dwError);

cleanup:
    return dwError;

error:
    goto cleanup;
}

//****************************************************************************************
//
//****************************************************************************************
DWORD CreateNewUserKeytabFile(AdtActionTP action)
{
   DWORD dwError = 0;
   AppContextTP appContext = (AppContextTP) ((AdtActionBaseTP) action)->opaque;

   dwError = CreateUserKeytabEntry(appContext, action->newUser.dn, action->newUser.name,
                                   action->newUser.password, action->newUser.keytab);
   ADT_BAIL_ON_ERROR(dwError);

cleanup:
    return dwError;

error:
    goto cleanup;
}


//****************************************************************************************
//
//****************************************************************************************
DWORD CreateNewComputerKeytabFile(AdtActionTP action)
{
   DWORD dwError = 0;
   AppContextTP appContext = (AppContextTP) ((AdtActionBaseTP) action)->opaque;
   PSTR pszServicePrincipalNameList = NULL;

   if (action->newComputer.servicePrincipalNameList)
      LwAllocateString(action->newComputer.servicePrincipalNameList, &pszServicePrincipalNameList);
   else
      LwAllocateString(DEFAULT_SERVICE_PRINCIPAL_NAME_LIST, &pszServicePrincipalNameList);

   PrintStdout(appContext,
               LogLevelVerbose,
               "%s: CreateNewComputerKeytabFile. DN=%s Name=%s Password=%s sAMAccountName=%s DnsHostName=%s Service Principal Name:%s\n",
               appContext->actionName,
               action->newComputer.dn,
               action->newComputer.name,
               action->newComputer.password,
               action->newComputer.namePreWin2000,
               action->newComputer.dnsHostName,
               pszServicePrincipalNameList);

   dwError = CreateComputerKeytabEntry(appContext, action->newComputer.dn, action->newComputer.name, 
                                       action->newComputer.password, action->newComputer.namePreWin2000,
                                       action->newComputer.dnsHostName, action->newComputer.keytab,
                                       pszServicePrincipalNameList);
   ADT_BAIL_ON_ERROR(dwError);

cleanup:

    LW_SAFE_FREE_STRING(pszServicePrincipalNameList);
    return dwError;

error:
    goto cleanup;
}


//****************************************************************************************
// If the given keytab file exists
//    If the original backup (keytab.lwidentity.orig) does exists
//      copy keytab file to keytab.lwidentity.bak 
//    else
//      copy keytab file keytab.lwidentify.orig
// endif
//****************************************************************************************
static
DWORD BackupKeytabFile(PSTR pszKtPath)
{
   DWORD dwError = 0;
   BOOLEAN bFileExists = FALSE;
   PSTR pszPostFixOriginal = ".lwidentity.orig";
   PSTR pszPostFixBak = ".lwidentity.bak";
   PSTR pszOrigFile = NULL;
   PSTR pszBakFile = NULL;

   // Check if the file provided  on the command line exists.
   dwError = LwCheckFileExists(pszKtPath, &bFileExists);
   ADT_BAIL_ON_ERROR_NP(dwError);

   if (!bFileExists)
   {
      // Nothing to do.
      goto cleanup;
   }

   dwError = LwAllocateStringPrintf(&pszOrigFile, "%s%s", pszKtPath, pszPostFixOriginal);
   ADT_BAIL_ON_ERROR_NP(dwError);


   // If we already made a copy of the original file, then don't overwrite it. Create a .bak
   // file instead.
   dwError = LwCheckFileExists(pszOrigFile, &bFileExists);
   ADT_BAIL_ON_ERROR_NP(dwError);

   if (!bFileExists)
   {
      // Make a copy of the original file.
      dwError = LwCopyFileWithOriginalPerms(pszKtPath, pszOrigFile);
      ADT_BAIL_ON_ERROR_NP(dwError);
   }
   else
   {
      dwError = LwAllocateStringPrintf(&pszBakFile, "%s%s", pszKtPath, pszPostFixBak);
      ADT_BAIL_ON_ERROR_NP(dwError);

      dwError = LwCopyFileWithOriginalPerms(pszKtPath, pszBakFile);
      ADT_BAIL_ON_ERROR_NP(dwError);
   }


cleanup:

    if (pszOrigFile)
    {
       LwFreeString(pszOrigFile);
       pszOrigFile = NULL;
    }

    if (pszBakFile)
    {
       LwFreeString(pszBakFile);
       pszBakFile = NULL;
    }

    return dwError;

error:
    goto cleanup;
}

//****************************************************************************************
// In order to generate user keytab entries in a keytab file the following information is needed:
// username: provided by command line.
// Knvo: Ldap query using sAMAccountName (username) to retrieve msDS-KeyVersionNumber
// Salt: Ldap query using sAMAccountName (username) to retrieve the userPrincipalName. If the 
//       UPN is empty/null then the salt for user account is 
//       < DNS of the realm, converted to upper case> | <user name>
//       See: https://msdn.microsoft.com/en-us/library/cc233883.aspx
// Principal name: 
// DC Name: Retrieved from lsass for the domain.
// Password: provided by command line.
// Keytab path: Keytab file path. If the file already exists, a backup is first created.
//****************************************************************************************
static
DWORD CreateUserKeytabEntry(AppContextTP appContext, PSTR pszDN, PSTR pszName, PSTR pszPass, PSTR pszKeytab)
{
    DWORD dwError = 0;
    size_t sPassLen = 0;
    PSTR pszDn = NULL;
    PSTR pszDcName = NULL;
    PSTR pszDcAddress = NULL;
    PSTR pszBaseDn = NULL;
    PSTR pszPrincipal = NULL;
    PSTR pszUserName = NULL;
    PSTR pszDomainFromDN = NULL;
    PSTR pszSalt = NULL;
    PWSTR pwszPass = NULL;
    PWSTR pwszDomainFromDN = NULL;
    PWSTR pwszDcName = NULL;
    PWSTR pwszKtPath = NULL;
    PWSTR pwszSalt = NULL;
    PWSTR pwszUserName = NULL;
    PWSTR pwszPrincipal = NULL;
    DWORD dwKnvo = 0;

    PrintStdout(appContext, LogLevelVerbose, "Creating user keytab entry.\n\tDN=%s\n\tuser=%s\n\tpassword=%s\n\tkeytab-file=%s\n", 
                  pszDN,
                  pszName,
                  pszPass,
                  pszKeytab);

    // Need the password argument sinces its combined with the salt.
    if (!pszPass)
    {
        dwError = ADT_ERR_ARG_MISSING_PASSWD;
        ADT_BAIL_ON_ERROR_NP(dwError);
    }

    dwError = LwAllocateString(pszDN, &pszDn);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwAllocateString(pszName, &pszUserName);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwMbsToWc16s(pszUserName, &pwszUserName);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwMbsToWc16s(pszKeytab, &pwszKtPath);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwMbsToWc16s(pszPass, &pwszPass);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwWc16sLen(pwszPass, &sPassLen);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = GetDomainFromDN(pszDn, &pszDomainFromDN);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwMbsToWc16s(pszDomainFromDN, &pwszDomainFromDN);
    ADT_BAIL_ON_ERROR(dwError);

    PrintStdout(appContext, LogLevelVerbose, "\tDomain=%s\n",  pszDomainFromDN);

    LwStrToLower(pszDomainFromDN);

    //
    // Use the given DN and find the domain controller name. Its needed for ldap queries.
    //
    dwError = GetDomainDcName(appContext, pszDomainFromDN, &pszDcName);
    ADT_BAIL_ON_ERROR(dwError);

    // The domain controller name is used for ldap queries.
    dwError = LwMbsToWc16s(pszDcName, &pwszDcName);
    ADT_BAIL_ON_ERROR(dwError);

    PrintStdout(appContext, LogLevelVerbose, "\tDCName=%s\n",  pszDcName);

    dwError = KtLdapGetBaseDnA(pszDcName, &pszBaseDn);
    ADT_BAIL_ON_ERROR(dwError);

    PrintStdout(appContext, LogLevelVerbose, "\tBaseDN=%s\n",  pszBaseDn);

    //
    // The principal name is username@DOMAIN.NET
    //
    dwError = KtKrb5FormatPrincipalW(pwszUserName, pwszDomainFromDN, &pwszPrincipal);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwWc16sToMbs(pwszPrincipal, &pszPrincipal);
    ADT_BAIL_ON_ERROR(dwError);

    PrintStdout(appContext, LogLevelVerbose, "\tPrincipal=%s\n",  pszPrincipal);

    //
    // The user's key version is retrieved from AD and used when creating the keytab entry later on.
    //   
    dwError = KtLdapGetKeyVersionA(pszDcName, pszBaseDn, pszPrincipal, &dwKnvo);
    ADT_BAIL_ON_ERROR(dwError);

    PrintStdout(appContext, LogLevelVerbose, "\tknvo=%d\n",  dwKnvo);

    // The salt can come from one of two places. Either from AD or generated locally.
    dwError = KtKrb5GetUserSaltingPrincipalA(pszUserName, NULL, pszDcName, pszBaseDn, &pszSalt); 
    ADT_BAIL_ON_ERROR(dwError);

    PrintStdout(appContext, LogLevelVerbose, "\tSalt=%s\n",  pszSalt);

    dwError = LwMbsToWc16s(pszSalt, &pwszSalt);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = BackupKeytabFile(pszKeytab);
    ADT_BAIL_ON_ERROR(dwError);

    // Several keytab entries are written. An entry for each of supported encryption types.
    dwError = KtKrb5AddKeyW( pwszPrincipal, pwszPass, sPassLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
    ADT_BAIL_ON_ERROR(dwError);

cleanup:

    LW_SAFE_FREE_MEMORY(pszDn);
    LW_SAFE_FREE_MEMORY(pszUserName);
    LW_SAFE_FREE_MEMORY(pwszUserName);
    LW_SAFE_FREE_MEMORY(pszSalt);
    LW_SAFE_FREE_MEMORY(pwszSalt);
    LW_SAFE_FREE_MEMORY(pszBaseDn);
    LW_SAFE_FREE_MEMORY(pszDcName);
    LW_SAFE_FREE_MEMORY(pszDomainFromDN);
    LW_SAFE_FREE_MEMORY(pwszDcName);
    LW_SAFE_FREE_MEMORY(pwszPass);
    LW_SAFE_FREE_MEMORY(pwszDomainFromDN);
    LW_SAFE_FREE_MEMORY(pszDcAddress);
    LW_SAFE_FREE_MEMORY(pszPrincipal);
    LW_SAFE_FREE_MEMORY(pwszPrincipal);
    LW_SAFE_FREE_MEMORY(pwszKtPath);

    return dwError;

error:
    goto cleanup;
}

//****************************************************************************************
//
//****************************************************************************************
static
DWORD CreateComputerKeytabEntry(AppContextTP appContext, PSTR pszDn, PSTR pszMachineName, 
                                PSTR pszPassword, PSTR pszMachineAccountName, PSTR pszDnsHostName,
                                PSTR pszKeytab, PSTR pszServicePrincipalNameList)
{
    DWORD dwError = 0;
    size_t sPasswordLen = 0;
    PSTR pszDomainFromDn = NULL;
    PSTR pszBaseDn = NULL;
    PSTR pszDcName = NULL;
    PSTR pszPrincipal = NULL;
    PSTR pszFqdn = NULL;
    PSTR pszSalt = NULL;
    PSTR pszServicePrincipalList = NULL;
    PSTR pszServicePrincipalListSave = NULL;
    PSTR pszSpn = NULL;
    PWSTR pwszSalt = NULL;
    PWSTR pwszFqdn = NULL;
    PWSTR pwszDcName = NULL;
    PWSTR pwszPrincipal = NULL;
    PWSTR pwszDomainFromDn = NULL;
    PWSTR pwszDomainFromDnUc = NULL;
    PWSTR pwszDomainFromDnLc = NULL;
    PWSTR pwszMachineName = NULL;
    PWSTR pwszMachineNameLc = NULL;
    PWSTR pwszMachineNameUc = NULL;
    PWSTR pwszMachineAccountName = NULL;
    PWSTR pwszPassword = NULL;
    PWSTR pwszBaseDn = NULL;
    PWSTR pwszKtPath = NULL;
    PWSTR pwszHostMachineUc = NULL;
    PWSTR pwszHostMachineLc = NULL;
    PWSTR pwszBuffer = NULL;
    PWSTR pwszSpn = NULL;
    DWORD dwKnvo = 0;
    BOOLEAN bIsServiceClass = FALSE;
    BOOLEAN bDirExists = FALSE;
    PSTR pszTmpKeytab = NULL;
    PSTR pszDirName = NULL;
    DWORD dwRetries = 0;
    DWORD dwMaxRetries = 2;

    wchar_t wszFqdnFmt[] = L"%ws/%ws.%ws@%ws";
    wchar_t wszFmt[] = L"%ws/%ws@%ws";
    wchar_t wszFmt2[] = L"%ws@%ws";


    // Check if the directory exists. If not, create it.
    dwError = LwStrDupOrNull(pszKeytab, &pszTmpKeytab);
    ADT_BAIL_ON_ERROR(dwError);
     
    pszDirName = dirname(pszTmpKeytab);
    dwError = LwCheckDirectoryExists(pszDirName, &bDirExists);
    ADT_BAIL_ON_ERROR(dwError);
   
    if (!bDirExists)
    {
       dwError = LwCreateDirectory(pszDirName, 0755);
       ADT_BAIL_ON_ERROR_STR(dwError, "Failed to create directory");
    }

    PrintStdout(appContext, LogLevelVerbose, "\tServicePrincipalName=%s\n",  pszServicePrincipalNameList);
    PrintStdout(appContext, LogLevelVerbose, "\tMachineName=%s\n",  pszMachineName);
    PrintStdout(appContext, LogLevelVerbose, "\tMachineAccountName=%s\n",  pszMachineAccountName);

    dwError = GetDomainFromDN(pszDn, &pszDomainFromDn);
    ADT_BAIL_ON_ERROR(dwError);
    PrintStdout(appContext, LogLevelVerbose, "\tDomainFromDn=%s\n",  pszDomainFromDn);

    //
    // Use the given DN and find the domain controller name. Its needed for ldap queries.
    //
    dwError = GetDomainDcName(appContext, pszDomainFromDn, &pszDcName);
    ADT_BAIL_ON_ERROR(dwError);
    PrintStdout(appContext, LogLevelVerbose, "\tDCName=%s\n",  pszDcName);

    dwError = KtLdapGetBaseDnA(pszDcName, &pszBaseDn);
    ADT_BAIL_ON_ERROR(dwError);
    PrintStdout(appContext, LogLevelVerbose, "\tBaseDN=%s\n",  pszBaseDn);

    dwError = LwMbsToWc16s(pszBaseDn, &pwszBaseDn);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwMbsToWc16s(pszPassword, &pwszPassword);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwWc16sLen(pwszPassword, &sPasswordLen);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwMbsToWc16s(pszKeytab, &pwszKtPath);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwMbsToWc16s(pszMachineAccountName, &pwszMachineAccountName);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwMbsToWc16s(pszMachineName, &pwszMachineName);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwAllocateWc16String(&pwszMachineNameLc, pwszMachineName);
    ADT_BAIL_ON_ERROR(dwError);
    LwWc16sToLower(pwszMachineNameLc);

    dwError = LwAllocateWc16String(&pwszMachineNameUc, pwszMachineName);
    ADT_BAIL_ON_ERROR(dwError);
    LwWc16sToUpper(pwszMachineNameUc);

    dwError = LwMbsToWc16s(pszDomainFromDn, &pwszDomainFromDn);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwAllocateWc16String(&pwszDomainFromDnLc, pwszDomainFromDn);
    ADT_BAIL_ON_ERROR(dwError);

    LwWc16sToLower(pwszDomainFromDnLc);

    dwError = LwAllocateWc16String(&pwszDomainFromDnUc, pwszDomainFromDn);
    ADT_BAIL_ON_ERROR(dwError);

    LwWc16sToUpper(pwszDomainFromDnUc);

    // The domain controller name is used for ldap queries.
    dwError = LwMbsToWc16s(pszDcName, &pwszDcName);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwAllocateWc16sPrintfW( &pwszFqdn, L"%ws.%ws", pwszMachineNameLc, pwszDomainFromDnLc);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwWc16sToMbs(pwszFqdn, &pszFqdn);
    ADT_BAIL_ON_ERROR(dwError);
    PrintStdout(appContext, LogLevelVerbose, "\tFqdn=%s\n",  pszFqdn);

    // Find the current key version number for machine account
    dwError = KtKrb5FormatPrincipalW(pwszMachineAccountName, pwszDomainFromDnUc, &pwszPrincipal);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwWc16sToMbs(pwszPrincipal, &pszPrincipal);
    ADT_BAIL_ON_ERROR(dwError);
    PrintStdout(appContext, LogLevelVerbose, "\tPrincipal=%s\n",  pszPrincipal);

    dwRetries = 0;
    do
    {
       dwError = KtLdapGetKeyVersionA(pszDcName, pszBaseDn, pszPrincipal, &dwKnvo);
       if (dwError == ERROR_FILE_NOT_FOUND)
       {
          // Possible race condition. The computer object may not be fully ready/created
          // in AD. Pause and try again.
          dwRetries++;
          sleep(3); 
       }
    } while ((dwError == ERROR_FILE_NOT_FOUND) && (dwRetries < dwMaxRetries));

    if (dwError == ERROR_FILE_NOT_FOUND)
    {
        dwError = LW_ERROR_SUCCESS;
        dwKnvo = 2;
        PrintStdout(appContext, LogLevelError, "Failed to get kvno from AD. Assuming kvno of 2");
    }
    ADT_BAIL_ON_ERROR(dwError);

    PrintStdout(appContext, LogLevelVerbose, "\tknvo=%d\n",  dwKnvo);

    // The salt can come from one of two places. Either from AD or generated locally.
    dwError = KtKrb5GetSaltingPrincipalW(pwszMachineName, pwszMachineAccountName,
                                         pwszDomainFromDn, pwszDomainFromDnUc, 
                                         pwszDcName, pwszBaseDn, &pwszSalt);
    ADT_BAIL_ON_ERROR(dwError);

    if (pwszSalt == NULL)
    {
        dwError = LwAllocateWc16String(&pwszSalt, pwszPrincipal);
        ADT_BAIL_ON_ERROR(dwError);
    }

    dwError = LwWc16sToMbs(pwszSalt, &pszSalt);
    ADT_BAIL_ON_ERROR(dwError);
    PrintStdout(appContext, LogLevelVerbose, "\tSalt=%s\n",  pszSalt);

    dwError = BackupKeytabFile(pszKeytab);
    ADT_BAIL_ON_ERROR_STR(dwError, "Failed to create backup keytab file");

    // Several keytab entries are written. An entry for each of supported encryption types.

    // Format: COMPUTERNAME$@REALM.NET
    //  
    dwError = KtKrb5AddKeyW( pwszPrincipal, pwszPassword, sPasswordLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
    ADT_BAIL_ON_ERROR_STR(dwError, "Failed to add entry to keytab file");

    // Format:  computername@REALM.NET
    //
    dwError = KtKrb5FormatPrincipalW(pwszMachineNameLc, pwszDomainFromDnUc, &pwszBuffer);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = KtKrb5AddKeyW(pwszBuffer, pwszPassword, sPasswordLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
    ADT_BAIL_ON_ERROR(dwError);

    pszServicePrincipalList = pszServicePrincipalNameList;
    pszSpn = strtok_r(pszServicePrincipalList, ",", &pszServicePrincipalListSave);
    while (pszSpn)
    {
        if (pwszSpn)
        {
           LW_SAFE_FREE_MEMORY(pwszSpn);
           pwszSpn = NULL;
        }

        // Strip any trailing slash from pszSpn and determine if its a full SPN or a service class.
        // Distinguish between nfs, nfs/ and http/www.linuxcomputer.com.
        GroomSpn(&pszSpn, &bIsServiceClass);

        dwError = LwMbsToWc16s(pszSpn, &pwszSpn);
        ADT_BAIL_ON_ERROR(dwError);

        if (!bIsServiceClass)
        {
           // Add pszSpn as is with the realm appended.    
           LW_SAFE_FREE_MEMORY(pwszBuffer);
           pwszBuffer = NULL;
           dwError = LwAllocateWc16sPrintfW(&pwszBuffer, wszFmt2, pwszSpn, pwszDomainFromDnUc);
           ADT_BAIL_ON_ERROR(dwError);

           dwError = KtKrb5AddKeyW(pwszBuffer, pwszPassword, sPasswordLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
           ADT_BAIL_ON_ERROR(dwError);
        }
        else
        {
           //
           // Format: spn/COMPUTERNAME@REALM.NET
           //
           LW_SAFE_FREE_MEMORY(pwszBuffer);
           pwszBuffer = NULL;
           dwError = LwAllocateWc16sPrintfW(&pwszBuffer, wszFmt, pwszSpn, pwszMachineNameUc, pwszDomainFromDnUc);
           ADT_BAIL_ON_ERROR(dwError);

           dwError = KtKrb5AddKeyW(pwszBuffer, pwszPassword, sPasswordLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
           ADT_BAIL_ON_ERROR(dwError);

           // Format:  spn/computername@REALM.NEt
           //
           LW_SAFE_FREE_MEMORY(pwszBuffer);
           pwszBuffer = NULL;
           dwError = LwAllocateWc16sPrintfW(&pwszBuffer, wszFmt, pwszSpn, pwszMachineNameLc, pwszDomainFromDnUc);
           ADT_BAIL_ON_ERROR(dwError);

           dwError = KtKrb5AddKeyW(pwszBuffer, pwszPassword, sPasswordLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
           ADT_BAIL_ON_ERROR(dwError);

           //
           // Format: spn/computername.domain.net@REALM.NET
           //   
           LW_SAFE_FREE_MEMORY(pwszBuffer);
           pwszBuffer = NULL;
           dwError = LwAllocateWc16sPrintfW(&pwszBuffer, wszFqdnFmt, pwszSpn, pwszMachineNameLc, pwszDomainFromDnLc, pwszDomainFromDnUc);
           ADT_BAIL_ON_ERROR(dwError);

           dwError = KtKrb5AddKeyW(pwszBuffer, pwszPassword, sPasswordLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
           ADT_BAIL_ON_ERROR(dwError);

           //
           //Format:  spn/COMPUTERNAME.domain.net@REALM.NET
           //  
           LW_SAFE_FREE_MEMORY(pwszBuffer);
           pwszBuffer = NULL;
           dwError = LwAllocateWc16sPrintfW(&pwszBuffer, wszFqdnFmt, pwszSpn, pwszMachineNameUc, pwszDomainFromDnLc, pwszDomainFromDnUc);
           ADT_BAIL_ON_ERROR(dwError);

           dwError = KtKrb5AddKeyW(pwszBuffer, pwszPassword, sPasswordLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
           ADT_BAIL_ON_ERROR(dwError);

           //
           //Format:  spn/COMPUTERNAME.DOMAIN.NET@REALM.NET
           //
           LW_SAFE_FREE_MEMORY(pwszBuffer);
           pwszBuffer = NULL;
           dwError = LwAllocateWc16sPrintfW(&pwszBuffer, wszFqdnFmt, pwszSpn, pwszMachineNameUc, pwszDomainFromDnUc, pwszDomainFromDnUc);
           ADT_BAIL_ON_ERROR(dwError);

           dwError = KtKrb5AddKeyW(pwszBuffer, pwszPassword, sPasswordLen, pwszKtPath, pwszSalt, pwszDcName, dwKnvo);
           ADT_BAIL_ON_ERROR(dwError);
        }

        pszSpn = strtok_r(NULL, ",", &pszServicePrincipalListSave);
    }

cleanup:

    LW_SAFE_FREE_MEMORY(pszDomainFromDn);
    LW_SAFE_FREE_MEMORY(pszBaseDn);
    LW_SAFE_FREE_MEMORY(pszDcName);
    LW_SAFE_FREE_MEMORY(pszPrincipal);
    LW_SAFE_FREE_MEMORY(pszFqdn);
    LW_SAFE_FREE_MEMORY(pszSalt);
    LW_SAFE_FREE_MEMORY(pwszSalt);
    LW_SAFE_FREE_MEMORY(pwszFqdn);
    LW_SAFE_FREE_MEMORY(pwszDcName);
    LW_SAFE_FREE_MEMORY(pwszPrincipal);
    LW_SAFE_FREE_MEMORY(pwszDomainFromDn);
    LW_SAFE_FREE_MEMORY(pwszDomainFromDnUc);
    LW_SAFE_FREE_MEMORY(pwszDomainFromDnLc);
    LW_SAFE_FREE_MEMORY(pwszMachineName);
    LW_SAFE_FREE_MEMORY(pwszMachineNameLc);
    LW_SAFE_FREE_MEMORY(pwszMachineNameUc);
    LW_SAFE_FREE_MEMORY(pwszMachineAccountName);
    LW_SAFE_FREE_MEMORY(pwszHostMachineUc);
    LW_SAFE_FREE_MEMORY(pwszHostMachineLc);
    LW_SAFE_FREE_MEMORY(pwszBaseDn);
    LW_SAFE_FREE_MEMORY(pwszKtPath);
    LW_SAFE_FREE_MEMORY(pwszBuffer);
    LW_SAFE_FREE_MEMORY(pwszSpn);

    return dwError;

error:
    goto cleanup;
}


static
DWORD GetDomainDcName(AppContextTP appContext, PSTR pszDomainFromDN, PSTR *pszDcName)
{
    DWORD dwError = 0;
    PSTR dnsName = NULL;
    PSTR pszIpAddr = NULL;

    dwError = FindAdServer(pszDomainFromDN, &pszIpAddr, &dnsName);
    ADT_BAIL_ON_ERROR(dwError);

    dwError = LwAllocateString((PCSTR) dnsName, pszDcName);
    ADT_BAIL_ON_ERROR(dwError);

cleanup:

    LW_SAFE_FREE_STRING(pszIpAddr);
    LW_SAFE_FREE_STRING(dnsName);

    return dwError;

error:
    goto cleanup;
}
