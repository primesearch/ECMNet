/* HelperThread Class for ECMNet.

  Copyright 2011 Mark Rodenkirch

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; either version 2 of the License, or (at your
  option) any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; see the file COPYING.  If not, write to the Free
  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  02111-1307, USA.
*/

#include "HelperThread.h"

#ifdef WIN32
   static DWORD WINAPI HelperThreadEntryPoint(LPVOID threadInfo);
#else
   static void *HelperThreadEntryPoint(void *threadInfo);
#endif

#include "HTMLGenerator.h"
#include "SQLStatement.h"
#include "WorkSender.h"
#include "WorkReceiver.h"
#include "IAmAMaster.h"
#include "KeepAliveThread.h"
#include "Composite.h"

void  HelperThread::StartThread(globals_t *theGlobals, HelperSocket *theSocket)
{
   ip_Globals = theGlobals;

   ip_Log = theGlobals->p_Log;

   is_HTMLTitle = new char[strlen(theGlobals->s_HTMLTitle) + 1];
   strcpy(is_HTMLTitle, theGlobals->s_HTMLTitle);

   is_ProjectName = new char[strlen(theGlobals->s_ProjectName) + 1];
   strcpy(is_ProjectName, theGlobals->s_ProjectName);

   if (theGlobals->s_AdminPassword)
   {
      is_AdminPassword = new char[strlen(theGlobals->s_AdminPassword) + 1];
      strcpy(is_AdminPassword, theGlobals->s_AdminPassword);
   }
   else
      is_AdminPassword = 0;

   ip_Socket = theSocket;

   ip_DBInterface = new DBInterface(ip_Log, "database.ini");

   ip_ClientCounter = theGlobals->p_ClientCounter;

   ip_ClientCounter->IncrementValue();

#ifdef WIN32
   // Ignore the thread handle return since the parent process won't suspend
   // or terminate the thread.
   CreateThread(0,                        // security attributes (0=use parents)
                0,                        // stack size (0=use parents)
                HelperThreadEntryPoint,
                this,
                0,                        // execute immediately
                0);                       // don't care about the thread ID
#else
   pthread_create(&ih_thread, NULL, &HelperThreadEntryPoint, this);
   pthread_detach(ih_thread);
#endif
}

#ifdef WIN32
DWORD WINAPI HelperThreadEntryPoint(LPVOID threadInfo)
#else
static void *HelperThreadEntryPoint(void *threadInfo)
#endif
{
   HelperThread *helper;

   helper = (HelperThread *) threadInfo;

   helper->HandleRequest();

   helper->ReleaseResources();

   delete helper;

#ifdef WIN32
   return 0;
#else
   pthread_exit(0);
#endif
}

void  HelperThread::ReleaseResources(void)
{
   ip_ClientCounter->DecrementValue();

   if (ip_DBInterface->IsConnected())
      ip_DBInterface->Disconnect();

   ip_Socket->Close();

   if (is_HTMLTitle)
      delete is_HTMLTitle;
   if (is_ProjectName)
      delete is_ProjectName;
   if (is_AdminPassword)
      delete is_AdminPassword;

   delete ip_DBInterface;
   delete ip_Socket;
}

void  HelperThread::HandleRequest(void)
{
   char    *theMessage, tempMessage[500];
   WorkSender   *workSender;
   WorkReceiver *workReceiver;
   IAmAMaster   *master;

   theMessage = ip_Socket->Receive(20);

   if (!theMessage)
      return;

   if (!memcmp(theMessage, "GET ", 4))
   {
      if (ip_DBInterface->Connect(ip_Socket->GetSocketID()))
         SendHTML(theMessage);
      return;
   }

   if (!ip_Socket->ParseFirstMessage(theMessage))
      return;

   if (!ip_DBInterface->Connect(ip_Socket->GetSocketID()))
   {
      ip_Log->LogMessage("Not connected to database.  Rejecting client connection.");
      return;
   }

   if (memcmp(ip_Socket->GetClientVersion(), "2.x", 3))
      ip_Socket->Send("Connected to server version %s", ECMNET_VERSION);

   theMessage = ip_Socket->Receive(20);
   if (!theMessage)
      return;

   if (!memcmp(theMessage, "ADMIN_ACTIVE ", 13))
      ChangeStatus(theMessage+13, true);

   if (!memcmp(theMessage, "ADMIN_INACTIVE ", 15))
      ChangeStatus(theMessage+15, false);

   if (!memcmp(theMessage, "ADMIN_COMPOSITE ", 16))
      AddComposites(theMessage+16);
   
   if (!memcmp(theMessage, "ADMIN_CURVES ", 13))
      AddCurves(theMessage+13);

   if (!memcmp(theMessage, "GETWORK", 7))
   {
      strcpy(tempMessage, theMessage);
      workSender = new WorkSender(ip_DBInterface, ip_Socket, ip_Log,
                                  ip_Globals->d_MinB1,
                                  ip_Globals->d_MaxB1,
                                  ip_Globals->i_PM1DepthOverECM,
                                  ip_Globals->i_PP1DepthOverECM,
                                  ip_Globals->p_B1,
                                  ip_Globals->i_B1Count,
                                  ip_Globals->p_TotalWorkSent,
                                  ip_Globals->p_Strategy,
                                  ip_Globals->p_Locker);
      workSender->ProcessMessage(tempMessage);
      delete workSender;

      if (!memcmp(ip_Socket->GetClientVersion(), "2.x", 3))
      {
         SendFile("Greeting.txt");
         ip_Socket->Send("Consider upgrading to v3.x");
         ip_Socket->Send("You can d/l the latest version from http://home.roadrunner.com/mrodenkirch/ecmnet.html");
         ip_Socket->Send("OK");
         ip_Socket->Receive();
      }
      else
      {
         ip_Socket->Send("Start Greeting");
         SendFile("Greeting.txt");
         ip_Socket->Send("End Greeting");
      }
   }

   if (!memcmp(theMessage, "RETURNWORK", 10))
   {
      strcpy(tempMessage, theMessage);
      workReceiver = new WorkReceiver(ip_DBInterface, ip_Socket, ip_Log, ip_Globals->s_ServerID, ip_Globals->b_Recurse);
      workReceiver->ProcessMessageOld(tempMessage);
      delete workReceiver;
   }

   if (!memcmp(theMessage, "Start Return", 12))
   {
      workReceiver = new WorkReceiver(ip_DBInterface, ip_Socket, ip_Log, ip_Globals->s_ServerID, ip_Globals->b_Recurse);
      workReceiver->ProcessMessage();
      delete workReceiver;
   }

   if (!memcmp(theMessage, "Start Sync", 10))
   {
      strcpy(tempMessage, theMessage);
      master = new IAmAMaster(ip_DBInterface, ip_Socket, ip_Log, ip_Globals->s_ServerID, ip_Globals->s_MasterPassword, ip_Globals->b_IAmASlave);
      master->SyncWithSlave(tempMessage);
      delete master;
   }
}

void  HelperThread::SendHTML(const char *theMessage)
{
   char                  thePage[200];
   HTMLGenerator        *htmlGenerator;

   if (strstr(theMessage, ".htm"))
      sscanf(theMessage, "GET /%s ", thePage);
   else if (strstr(theMessage, ".ico"))
      sscanf(theMessage, "GET /%s ", thePage);
   else
      thePage[0] = 0;

   while (theMessage)
   {
      theMessage = ip_Socket->Receive();

      if (!theMessage)
         break;

      if (!memcmp(theMessage, "Connection", 10))
         break;
   }

   htmlGenerator = new HTMLGenerator();
   htmlGenerator->SetDBInterface(ip_DBInterface);
   htmlGenerator->SetLog(ip_Log);
   htmlGenerator->SetHTMLTitle(is_HTMLTitle);
   htmlGenerator->SetProjectName(is_ProjectName);
   htmlGenerator->SetB1Info(ip_Globals->d_MinB1, ip_Globals->p_B1, ip_Globals->i_B1Count);

   htmlGenerator->Send(ip_Socket, thePage);

   delete htmlGenerator;
}

bool     HelperThread::VerifyAdminPassword(const char *password)
{
   if (!strcmp(password, is_AdminPassword))
   {
      ip_Socket->Send("OK: Password Verified");
      ip_Log->LogMessage("%s at %s is connecting as admin", ip_Socket->GetEmailID(), ip_Socket->GetFromAddress());
      return true;
   }
   else
   {
      ip_Socket->Send("ERR: The entered password is not valid.  No access will be granted.");
      ip_Log->LogMessage("%s at %s attempted to connect as admin, but was rejected", ip_Socket->GetEmailID(), ip_Socket->GetFromAddress());
      return false;
   }
}

// Send the specified file to the client
void     HelperThread::SendFile(const char *fileName)
{
   FILE *fp;
   char *line;

   if ((fp = fopen(fileName, "rt")) == NULL)
   {
      ip_Log->LogMessage("Could not open file: %s", fileName);
      return;
   }

   line = new char [BUFFER_SIZE];

   ip_Socket->StartBuffering();
   while (fgets(line, BUFFER_SIZE, fp) != NULL)
   {
      StripCRLF(line);

      if (strlen(line) > 0)
         ip_Socket->Send("%s", line);
   }

   delete [] line;
   fclose(fp);

   ip_Socket->SendBuffer();
}

void  HelperThread::ChangeStatus(const char *theMessage, bool makeActive)
{
   SQLStatement  *selectStatement;
   SQLStatement  *updateStatement;
   const char *selectSQL = "select IsActive from MasterComposite where MasterCompositeName = ? ";
   const char *updateSQL = "update MasterComposite set IsActive = ? where MasterCompositeName = ? ";
   char     password[100];
   char     compositeName[NAME_LENGTH+1];
   int32_t  isActive;

   if (ip_Globals->b_IAmASlave)
   {
      ip_Socket->Send("Cannot use ADMIN functions on a slave server");
      return;
   }

   if (sscanf(theMessage, "%s %s", password, compositeName) != 2)
   {
      ip_Socket->Send("ADMIN Message format incorrect");
      return;
   }

   if (!VerifyAdminPassword(password))
      return;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(compositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(&isActive);

   if (selectStatement->FetchRow(true))
   {
      updateStatement = new SQLStatement(ip_Log, ip_DBInterface, updateSQL);
      updateStatement->BindInputParameter(compositeName, NAME_LENGTH);
      updateStatement->BindInputParameter(makeActive);
      if (!updateStatement->Execute())
      {
         ip_Socket->Send("The server encounted an SQL error when changing the status of %s", compositeName);
         ip_DBInterface->Rollback();
      }
      else
         ip_DBInterface->Commit();

      delete updateStatement;
   }
   else
   {
      ip_Socket->Send("Composite %s not found", compositeName);
   }

   delete selectStatement;
}

void  HelperThread::AddComposites(const char *thePassword)
{
   SQLStatement  *selectStatement;
   const char *selectSQL = "select IsActive from MasterComposite where MasterCompositeValue = ? ";
   char    *readBuf, *pos;
   char     compositeName[NAME_LENGTH+1];
   char     compositeValue[VALUE_LENGTH+1];
   int32_t  isActive, added;
   bool     found;
   Composite *theComposite;
   MasterComposite *theMasterComposite;

   if (ip_Globals->b_IAmASlave)
   {
      ip_Socket->Send("Cannot use ADMIN functions on a slave server");
      return;
   }

   if (!VerifyAdminPassword(thePassword))
      return;

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter(compositeValue, VALUE_LENGTH, false);
   selectStatement->BindSelectedColumn(&isActive);

   added = 0;
   readBuf = ip_Socket->Receive(10);
   while (readBuf)
   {
      if (!strcmp(readBuf, "No more composites"))
         break;

      found = true;
      pos = strchr(readBuf, ' ');

      if (!pos)
      {
         strcpy(compositeName, readBuf);
         strcpy(compositeValue, readBuf);
      }
      else
      {
         *pos = 0;
         strcpy(compositeName, readBuf);
         strcpy(compositeValue, pos+1);
      }

      selectStatement->SetInputParameterValue(compositeValue, true);

      theComposite = new Composite(ip_DBInterface, ip_Log, compositeName, false);
      found = theComposite->WasFound();
      if (found)
      {
         ip_Socket->Send("Composite %s already in the database", compositeName);
         delete theComposite;
      }
      else
      {
         found = selectStatement->FetchRow(true);
         if (found)
         {
            ip_Socket->Send("Composite %s already in the database under a different name", compositeValue);
            delete theComposite;
         }
      }

      if (!found)
      {
         theMasterComposite = new MasterComposite(ip_DBInterface, ip_Log, compositeName);
         theMasterComposite->SetValue(compositeValue);
         theComposite->SetValue(compositeValue);

         if (theComposite->GetPRPTestResult() != PRPTEST_AS_COMPOSITE)
            ip_Socket->Send("Number %s appears to be PRP or prime.  It was not added.", compositeValue);
         else
         {
            theComposite->SetMasterCompositeName(compositeName);
            if (theMasterComposite->FinalizeChanges() && theComposite->FinalizeChanges())
            {
               ip_DBInterface->Commit();
               added++;
            }
            else
            {
               ip_DBInterface->Rollback();
               ip_Socket->Send("The server encountered an error inserting composite %s", compositeName);
            }
         }
         delete theComposite;
         delete theMasterComposite;
      }

      readBuf = ip_Socket->Receive(10);
   }

   delete selectStatement;

   ip_Socket->Send("%d composites were added to the server", added);
   ip_Socket->Send("Done");
}

void  HelperThread::AddCurves(const char *thePassword)
{
   char    *readBuf, *pos1, *pos2, *pos3;
   char     compositeName[NAME_LENGTH+1];
   char     factorMethod[METHOD_LENGTH+1];
   double   b1;
   long     curvesDone;
   int32_t  added;
   bool     success;
   Composite *composite;
   MasterComposite *masterComposite;

   if (ip_Globals->b_IAmASlave)
   {
      ip_Socket->Send("Cannot use ADMIN functions on a slave server");
      return;
   }

   if (!VerifyAdminPassword(thePassword))
      return;


   added = 0;
   readBuf = ip_Socket->Receive(10);
   while (readBuf)
   {
      if (!strcmp(readBuf, "No more curves"))
         break;

      b1 = 0.0;
      curvesDone = 0;
      success = true;
      pos1 = strchr(readBuf, ' ');

      if (pos1)
      {
         *pos1 = 0;
         strcpy(compositeName, readBuf);
         pos2 = strchr(pos1+1, ' ');

         if (pos2)
         {
            *pos2 = 0;
            strcpy(factorMethod, pos1+1);
            pos3 = strchr(pos2+1, ' ');
            if (pos3) 
            {
               curvesDone = atol(pos2+1);
               b1 = atof(pos3+1);
            }
         }
      }

      if (!pos1 || b1 == 0.0 || curvesDone == 0)
      {
         ip_Socket->Send("Curve input format is invalid");
         continue;
      }

      composite = new Composite(ip_DBInterface, ip_Log, compositeName, true);
      if (!composite->WasFound())
      {
         ip_Socket->Send("Composite %s not found", compositeName);
      }
      else
      {
         masterComposite = new MasterComposite(ip_DBInterface, ip_Log, composite->GetMasterCompositeName());

         success = masterComposite->AddWorkDone(factorMethod, b1, curvesDone);
         if (success)
         {
            success = masterComposite->SendUpdatesToAllSlaves();

            masterComposite->SetSendUpdatesToMaster(true);
         }

         if (success) success = masterComposite->FinalizeChanges();

         if (success) ip_DBInterface->Commit();
         else ip_DBInterface->Rollback();

         delete masterComposite;
      }

      delete composite;

      readBuf = ip_Socket->Receive(10);
   }

   ip_Socket->Send("%d composites were added to the server", added);
   ip_Socket->Send("Done");
}