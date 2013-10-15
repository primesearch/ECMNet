/* HTMLGenerator Class for ECMNet.

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

#include "HTMLGenerator.h"
#include "SQLStatement.h"

#define SPLIT_LENGTH 80

typedef struct
{
   double   b1;                 // b1
   long     curvesECM;          // total ECM curves done for this B1
   long     curvesPP1;          // total P+1 curves done for this B1
   long     curvesPM1;          // total P-1 curves done for this B1
   void    *nextB1;
} b1temp_t;

void     HTMLGenerator::Send(Socket *theSocket, const char *thePage)
{
   theSocket->Send("HTTP/1.1 200 OK");
   theSocket->Send("Connection: close");

   // The client needs this extra carriage return before sending HTML
   theSocket->Send("\n");

   if (!strcmp(thePage, "user_stats.html"))
      UserStats(theSocket);
   else if (!strcmp(thePage, "pending_work.html"))
      PendingWork(theSocket);
   else if (!strcmp(thePage, "master_summary.html"))
      MasterSummary(theSocket, false);
   else if (!strcmp(thePage, "factor_summary.html"))
      MasterSummary(theSocket, true);
   else if (!strcmp(thePage, "composite_work.html"))
      CompositeInfo(theSocket, true);
   else if (!strcmp(thePage, "composite_summary.html"))
      CompositeInfo(theSocket, false);
   else {
      theSocket->Send("<html><head><title>ECMNet - %s</title>", is_HTMLTitle.c_str());
      theSocket->Send("<p align=center><b><font size=6>%s</font></b></p>", is_ProjectName.c_str());
      if (!strlen(thePage)) {
   theSocket->Send("<A Href=\"user_stats.html\">User Stats</A><CR>\n");
   theSocket->Send("<A Href=\"pending_work.html\">Pending Work</A><CR>\n");
   theSocket->Send("<A Href=\"factor_summary.html\">Factor Summary</A><CR>\n");
   theSocket->Send("<A Href=\"composite_work.html\">Composites</A><CR>\n");
   theSocket->Send("<A Href=\"composite_summary.html\"</A>Composite Summary<CR>\n");
      } else {
      theSocket->Send("<br><br><br><p align=center><b><font size=4 color=#FF0000>Page %s does not exist on this server.</font></b></p>", thePage);
      theSocket->Send("<p align=center><b><font size=4 color=#FF0000>Better luck next time!</font></b></p>");
      }
   }

   // The client needs this extra carriage return before closing the socket
   theSocket->Send("\n");
   return;
}

void     HTMLGenerator::CompositeInfo(Socket *theSocket, bool showDetails)
{
   SQLStatement *selectStatement;
   const char   *selectSQL = "select MasterComposite.MasterCompositeName, Composite.CompositeName, " \
                             "       Composite.CompositeValue, MasterComposite.TotalWorkDone, " \
                             "       Composite.DecimalLength " \
                             "  from MasterComposite, Composite " \
                             " where MasterComposite.IsActive = 1 " \
                             "   and Composite.FactorEventCount = 0 " \
                             "   and MasterComposite.MasterCompositeName = Composite.MasterCompositeName " \
                          " order by MasterComposite.DecimalLength, MasterComposite.MasterCompositeName ";
   char        masterCompositeName[NAME_LENGTH+1];
   char        compositeName[NAME_LENGTH+1];
   char        compositeValue[VALUE_LENGTH+1];
   int32_t     decimalLength;
   char       *output;
   double      workDone;

   if (showDetails)
      theSocket->Send("<html><head><title>ECMNet %s Composite Details - %s</title>", ECMNET_VERSION, is_HTMLTitle.c_str());
   else
      theSocket->Send("<html><head><title>ECMNet %s Composite Summary - %s</title>", ECMNET_VERSION, is_HTMLTitle.c_str());

   theSocket->Send("<p align=center><b><font size=6>%s</font></b></p></head>", is_ProjectName.c_str());

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindSelectedColumn(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(compositeValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&workDone);
   selectStatement->BindSelectedColumn(&decimalLength);

   if (!showDetails)
   {
      theSocket->Send("<p>&nbsp;<table frame=box align=center border=1>");
      theSocket->Send("<tr><td><b>Name<td><b>Length<td><b>Current ECM B1<td><b>Number</tr>");
   }

   while (selectStatement->FetchRow(false))
   {
      theSocket->StartBuffering();
      output = SplitAcrossLines(compositeValue);
      if (showDetails)
      {
         theSocket->Send("<p>&nbsp;<table frame=box align=center border=1>");
         theSocket->Send("<tr><td><b>Name<td><b>Length<td><b>Current ECM B1<td><b>Number</tr>");
         theSocket->Send("<tr><td>%s<td align=right>%d<td align=right>%.0lf<td>%s</tr>",
                         compositeName, decimalLength, DetermineB1(workDone), output);
         theSocket->Send("</table>");
      }
      else
         theSocket->Send("<tr><td>%s<td align=right>%d<td align=right>%.0lf<td>%s</tr>",
                         compositeName, decimalLength, DetermineB1(workDone), output);

      delete [] output;

      if (showDetails)
         CompositeWorkDone(theSocket, masterCompositeName);

      theSocket->SendBuffer();
   }

   if (!showDetails)
      theSocket->Send("</table>");

   theSocket->Send("</body></html>");
   delete selectStatement;
}

void     HTMLGenerator::CompositeWorkDone(Socket *theSocket, const char *masterCompositeName)
{
   SQLStatement *selectWorkStatement;
   const char   *selectWorkSQL = "select FactorMethod, B1, LocalCurves " \
                                 "  from MasterCompositeWorkDone " \
                                 " where MasterCompositeName = ? " \
                                 "order by B1, FactorMethod ";
   char        factorMethod[METHOD_LENGTH+1];
   double      b1;
   int32_t     curveCount;
   b1temp_t   *b1Ptr = 0, *lastB1, *useB1;
   bool        first;

   selectWorkStatement = new SQLStatement(ip_Log, ip_DBInterface, selectWorkSQL);
   selectWorkStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectWorkStatement->BindSelectedColumn(factorMethod, METHOD_LENGTH);
   selectWorkStatement->BindSelectedColumn(&b1);
   selectWorkStatement->BindSelectedColumn(&curveCount);

   while (selectWorkStatement->FetchRow(false))
   {
      if (!b1Ptr)
      {
         useB1 = new b1temp_t;
         useB1->nextB1 = 0;
         useB1->curvesECM = 0;
         useB1->curvesPM1 = 0;
         useB1->curvesPP1 = 0;

         b1Ptr = useB1;
      }
      else
      {
         useB1 = b1Ptr;
         lastB1 = b1Ptr;
         while (useB1)
         {
            if (useB1->b1 == b1)
               break;

            lastB1 = useB1;
            useB1 = (b1temp_t *) useB1->nextB1;
         }

         if (!useB1)
         {
            useB1 = new b1temp_t;
            useB1->nextB1 = 0;
            useB1->curvesECM = 0;
            useB1->curvesPM1 = 0;
            useB1->curvesPP1 = 0;

            lastB1->nextB1 = useB1;
         }
      }

      useB1->b1 = b1;
      if (!strcmp(factorMethod, FM_PM1))
         useB1->curvesPM1 = curveCount;
      if (!strcmp(factorMethod, FM_PP1))
         useB1->curvesPP1 = curveCount;
      if (!strcmp(factorMethod, FM_ECM))
         useB1->curvesECM = curveCount;
   }

   selectWorkStatement->CloseCursor();
   delete selectWorkStatement;

   first = true;

   while (b1Ptr)
   {
      useB1 = b1Ptr;

      if (first)
         {
            theSocket->Send("<table frame=box align=center border=1>");
            theSocket->Send("<tr><td><b>B1<td><b>ECM Curves<td><b>P_1 Tests<td><b>P-1 Tests</tr>");
            first = false;
         }

         theSocket->Send("<tr><td align=right>%.0lf<td align=right>%d<td align=right>%d<td align=right>%d</tr>", useB1->b1,
                         useB1->curvesECM, useB1->curvesPP1, useB1->curvesPM1);

      b1Ptr = (b1temp_t *) useB1->nextB1;
      free(useB1);
   }

   if (first)
   {
      theSocket->Send("<table frame=box align=center border=1>");
      theSocket->Send("<tr><td><b>No factoring work has been done for this candidate</tr>");
   }

   theSocket->Send("</table>");
}

void     HTMLGenerator::MasterSummary(Socket *theSocket, bool mastersWithFactors)
{
   SQLStatement *selectStatement;
   const char   *selectSQL = "select MasterCompositeName, MasterCompositeValue, FullyFactored, " \
                             "       IsActive, TotalWorkDone, FactorCount, DecimalLength " \
                             "  from MasterComposite " \
                             " where FactorCount > ? " \
                          " order by DecimalLength, MasterCompositeName ";
   char        masterCompositeName[NAME_LENGTH+1];
   char        masterCompositeValue[VALUE_LENGTH+1];
   int32_t     isActive, decimalLength, factorCount, fullyFactored;
   char        theClass, *output;
   double      workDone;

   if (mastersWithFactors)
      theSocket->Send("<html><head><title>ECMNet %s Factor Summary - %s</title>", ECMNET_VERSION, is_HTMLTitle.c_str());
   else
      theSocket->Send("<html><head><title>ECMNet %s Master Composite Summary - %s</title>", ECMNET_VERSION, is_HTMLTitle.c_str());
   theSocket->Send("<p align=center><b><font size=6>%s</font></b></p>", is_ProjectName.c_str());
   theSocket->Send("<style type=\"Text/css\">.b{background-color:aqua}.g{background-color:lime}.r{background-color:red}.l{text-align: left}td{text-align: center}");
   theSocket->Send("</style></head>\n");

   selectStatement = new SQLStatement(ip_Log, ip_DBInterface, selectSQL);
   selectStatement->BindInputParameter((mastersWithFactors ? 0 : -1));
   selectStatement->BindSelectedColumn(masterCompositeName, NAME_LENGTH);
   selectStatement->BindSelectedColumn(masterCompositeValue, VALUE_LENGTH);
   selectStatement->BindSelectedColumn(&fullyFactored);
   selectStatement->BindSelectedColumn(&isActive);
   selectStatement->BindSelectedColumn(&workDone);
   selectStatement->BindSelectedColumn(&factorCount);
   selectStatement->BindSelectedColumn(&decimalLength);

   if (!selectStatement->FetchRow(false))
   {
     theSocket->Send("<table frame=box align=center border=1>");
     theSocket->Send("<tr><th align=center>Factor Summary</tr>");
     theSocket->Send("<tr align=center><td>No factors found</tr>");
     theSocket->Send("</table>");
     selectStatement->CloseCursor();
     delete selectStatement;
     return;
   }

   theSocket->Send("<p><table frame=box align=center border=1><tr><td><b>Color legend:");
   theSocket->Send("<td class=b><b>factored<td class=g><b>active<TD class=r><b>inactive</table>");

   if (!mastersWithFactors)
   {
      theSocket->Send("<p>&nbsp;<table frame=box align=center border=1>");
      theSocket->Send("<tr><td><b>Name<td><b>Length<td><b>Factors<td><b>Current ECM B1<td><b>Number</tr>");
   }

   do {
      if (fullyFactored)
         theClass = 'b';
      else if (isActive)
         theClass = 'g';
      else
         theClass = 'r';

      theSocket->StartBuffering();
      output = SplitAcrossLines(masterCompositeValue);

      if (mastersWithFactors)
      {
         theSocket->Send("<p>&nbsp;<table frame=box align=center border=1>");
         theSocket->Send("<tr class=%c><td><b>Name<td><b>Length<td><b>Factors<td><b>Current ECM B1<td><b>Number</tr>", theClass);
      }
      theSocket->Send("<tr class=%c><td>%s<td>%d<td>%d<td>%.0lf<td>%s</tr>",
                      theClass, masterCompositeName, decimalLength, factorCount, DetermineB1(workDone), output);
      if (mastersWithFactors)
         theSocket->Send("</table>");

      delete [] output;

      if (mastersWithFactors && factorCount > 0)
         MasterFactorDetail(theSocket, masterCompositeName);

      theSocket->SendBuffer();
   } while (selectStatement->FetchRow(false));

   if (!mastersWithFactors)
      theSocket->Send("</table>");

   theSocket->Send("</body></html>");
   delete selectStatement;
}

// Show all factors for the master composite while avoiding sending composite co-factors that have
// had been factored
void     HTMLGenerator::MasterFactorDetail(Socket *theSocket, const char *masterCompositeName)
{
   SQLStatement *selectFactorStatement;
   const char   *selectFactorSQL = "select cfed.FactorValue, cfe.UserID, cfe.MachineID, cfe.InstanceID, cfe.ReportedDate, " \
                                   "       cfe.FactorMethod, cfe.B1, cfe.B1Time, cfe.B2, cfe.B2Time, " \
                                   "       cfe.Sigma, cfe.X0, cfed.DecimalLength, cfed.PRPTestResult " \
                                   "  from CompositeFactorEventDetail cfed, " \
                                   "       CompositeFactorEvent cfe, Composite " \
                                   " where Composite.MasterCompositeName = ? " \
                                   "   and Composite.CompositeName = cfe.CompositeName " \
                                   "   and Composite.CompositeName = cfed.CompositeName " \
                                   "   and cfe.FactorEvent = cfed.FactorEvent " \
                                   "   and not exists (select 'x' from Composite cmp " \
                                                       "where cmp.CompositeValue = FactorValue " \
                                                       "  and cmp.FactorEventCount > 0) " \
                                   " order by DecimalLength ";
   char        factorValue[VALUE_LENGTH+1];
   char        userID[ID_LENGTH+1];
   char        machineID[ID_LENGTH+1];
   char        instanceID[ID_LENGTH+1];
   char        factorMethod[METHOD_LENGTH+1];
   char        dtString[80];
   char        factorType[20];
   char       *output;
   bool        first;
   double      b1, b1Time, b2, b2Time;
   int32_t     decimalLength, prpTestResult;
   int64_t     reportedDate, sigma, x0;
   time_t      theDate;
   struct tm  *theTM;

   selectFactorStatement = new SQLStatement(ip_Log, ip_DBInterface, selectFactorSQL);
   selectFactorStatement->BindInputParameter(masterCompositeName, NAME_LENGTH);
   selectFactorStatement->BindSelectedColumn(factorValue, VALUE_LENGTH);
   selectFactorStatement->BindSelectedColumn(userID, ID_LENGTH);
   selectFactorStatement->BindSelectedColumn(machineID, ID_LENGTH);
   selectFactorStatement->BindSelectedColumn(instanceID, ID_LENGTH);
   selectFactorStatement->BindSelectedColumn(&reportedDate);
   selectFactorStatement->BindSelectedColumn(factorMethod, METHOD_LENGTH);
   selectFactorStatement->BindSelectedColumn(&b1);
   selectFactorStatement->BindSelectedColumn(&b1Time);
   selectFactorStatement->BindSelectedColumn(&b2);
   selectFactorStatement->BindSelectedColumn(&b2Time);
   selectFactorStatement->BindSelectedColumn(&sigma);
   selectFactorStatement->BindSelectedColumn(&x0);
   selectFactorStatement->BindSelectedColumn(&decimalLength);
   selectFactorStatement->BindSelectedColumn(&prpTestResult);

   first = true;
   while (selectFactorStatement->FetchRow(false))
   {
      if (first)
      {
         first = false;
         theSocket->Send("<table frame=box align=center border=1>");
         theSocket->Send("<tr><td>Factor Type<td>User<td>Machine<td>Instance<td>Method<td>B1<td>B2<td>Sigma/x0<td>Digits<td>Date Found<td>Time Used<td>Factor</tr>");
      }

      strcpy(factorType, "Unknown");
      if (prpTestResult == PRPTEST_AS_COMPOSITE) strcpy(factorType, "Composite");
      if (prpTestResult == PRPTEST_AS_PRP) strcpy(factorType, "PRP");
      if (prpTestResult == PRPTEST_AS_PRIME) strcpy(factorType, "Prime");

      theDate = reportedDate;
      theTM = localtime(&theDate);
      strftime(dtString, sizeof(dtString), "%c", theTM);

      output = SplitAcrossLines(factorValue);
      theSocket->Send("<tr><td>%s<td>%s<td>%s<td>%s<td>%s<td>%.0lf<td>%.0lf<td>%"PRId64"<td>%d<td>%s<td>%.lf seconds<td class=l>%s</tr>",
                      factorType,
                      userID,
                      machineID,
                      instanceID,
                      factorMethod,
                      b1,
                      b2,
                      (strcmp(factorMethod, FM_ECM) ? x0 : sigma),
                      decimalLength,
                      dtString,
                      (b1Time + b2Time),
                      output);
      delete [] output;
   }

   if (!first)
      theSocket->Send("</table>");

   selectFactorStatement->CloseCursor();
   delete selectFactorStatement;
}

void     HTMLGenerator::UserStats(Socket *theSocket)
{
   SQLStatement *sqlStatement;
   char        userID[201];
   int32_t     factorsFound;
   double      workDone, time;
   int64_t     days, hours, minutes, seconds;

   const char *theSelect = "select UserID, TotalWorkDone, TotalFactorsFound, TotalTime " \
                           "  from UserStats " \
                           "order by UserID ";

   theSocket->Send("<html><head><title>ECMNet %s User Statistics- %s</title>", ECMNET_VERSION, is_HTMLTitle.c_str());

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(userID, sizeof(userID));
   sqlStatement->BindSelectedColumn(&workDone);
   sqlStatement->BindSelectedColumn(&factorsFound);
   sqlStatement->BindSelectedColumn(&time);

   if (!sqlStatement->FetchRow(false))
   {
     theSocket->Send("<table frame=box align=center border=1>");
     theSocket->Send("<tr><th align=center>User Stats</tr>");
     theSocket->Send("<tr align=center><td>No user stats found</tr>");
     theSocket->Send("</table>");
   }
   else
   {
      theSocket->StartBuffering();
      theSocket->Send("<table frame=box align=center border=1>");
      theSocket->Send("<tr><th>User<th>Total Work<th>Factors Found<th>Total Time</tr>");

      do
      {
         seconds = (int64_t) time;
         days = seconds / (24 * 60 * 60);
         seconds = seconds - (days * 24 * 60 * 60);
         hours = seconds / (60 * 60);
         seconds = seconds - (hours * 60 * 60);
         minutes = seconds / 60;
         seconds = seconds % 60;

         workDone /= 1000000.0;
         if (days == 0)
            theSocket->Send("<tr><td>%s<td align=right>%9.2f<td align=right>%d<td align=right>%"PRId64":%.02lld:%.02lld</tr>",
                            userID, workDone, factorsFound, hours, minutes, seconds);
         else
            theSocket->Send("<tr><td>%s<td align=right>%9.2f<td align=right>%d<td align=right>%"PRId64" days %"PRId64":%.02lld:%.02lld</tr>",
                            userID, workDone, factorsFound, days, hours, minutes, seconds);
      } while (sqlStatement->FetchRow(false));

      theSocket->Send("</table>");
      theSocket->SendBuffer();
   }

   delete sqlStatement;
}

void     HTMLGenerator::PendingWork(Socket *theSocket)
{
   SQLStatement *sqlStatement;
   char        userID[ID_LENGTH+1];
   char        machineID[ID_LENGTH+1];
   char        instanceID[ID_LENGTH+1];
   char        compositeName[NAME_LENGTH+1];
   char        factorMethod[METHOD_LENGTH+1];
   char        dtString[80];
   double      b1;
   int32_t     curves;
   int64_t     dateAssigned;
   int32_t     seconds, hours, minutes;
   time_t      theTime;
   struct tm  *theTM;

   const char *theSelect = "select UserID, MachineID, InstanceID, CompositeName, FactorMethod, B1, Curves, DateAssigned " \
                           "  from PendingWork " \
                         "order by DateAssigned ";

   theSocket->Send("<html><head><title>ECMNet %s Pending Work - %s</title>", ECMNET_VERSION, is_HTMLTitle.c_str());

   sqlStatement = new SQLStatement(ip_Log, ip_DBInterface, theSelect);

   sqlStatement->BindSelectedColumn(userID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(machineID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(instanceID, ID_LENGTH);
   sqlStatement->BindSelectedColumn(compositeName, NAME_LENGTH);
   sqlStatement->BindSelectedColumn(factorMethod, METHOD_LENGTH);
   sqlStatement->BindSelectedColumn(&b1);
   sqlStatement->BindSelectedColumn(&curves);
   sqlStatement->BindSelectedColumn(&dateAssigned);

   if (!sqlStatement->FetchRow(false))
   {
     theSocket->Send("<table frame=box align=center border=1>");
     theSocket->Send("<tr><th align=center>Pending Work</tr>");
     theSocket->Send("<tr align=center><td>No pending work found</tr>");
     theSocket->Send("</table>");
   }
   else
   {
      theSocket->StartBuffering();
      theSocket->Send("<table frame=box align=center border=1>");
      theSocket->Send("<tr><th>User<th>Machine<th>Instance<th>Composite<th>Factor Method<th>B1<th>Curves<th>Date Assigned<th>Age</tr>");

      do
      {
         theTime = dateAssigned;
         theTM = localtime(&theTime);
         strftime(dtString, sizeof(dtString), "%c", theTM);
         seconds = (int32_t) (time(NULL) - theTime);
         hours = seconds / 3600;
         minutes = (seconds - hours * 3600) / 60;
         theSocket->Send("<tr><td>%s<td>%s<td>%s<td>%s<td>%s<td align=right>%.0lf<td align=right>%d<td align=center>%s<td>%d:%02d</tr>",
                         userID, machineID, instanceID, compositeName, factorMethod, b1, curves, dtString, hours, minutes);
      } while (sqlStatement->FetchRow(false));

      theSocket->Send("</table>");
      theSocket->SendBuffer();
   }

   delete sqlStatement;
}


// Given the amount of work done, determine the current b1 that currently needs more work
// and how many more curves need to be done for that b1.
double      HTMLGenerator::DetermineB1(double totalWorkDone)
{
   int32_t  b1Index;

   b1Index = 0;
   while (b1Index<ii_B1Count && (totalWorkDone > ip_B1[b1Index].sumWork || id_MinB1 > ip_B1[b1Index].b1))
      b1Index++;

   if (b1Index < ii_B1Count)
      return ip_B1[b1Index].b1;

   return 0.0;
}

char   *HTMLGenerator::SplitAcrossLines(char *input)
{
   char *output, *inputPtr, *outputPtr;

   output = new char[VALUE_LENGTH + 1000];
   inputPtr = input;
   outputPtr = output;
   while (strlen(inputPtr) > SPLIT_LENGTH)
   {
      memcpy(outputPtr, inputPtr, SPLIT_LENGTH);
      outputPtr += SPLIT_LENGTH;
      memcpy(outputPtr, "<br>", 4);
      outputPtr += 4;
      inputPtr += SPLIT_LENGTH;
   }

   memcpy(outputPtr, inputPtr, strlen(inputPtr));
   outputPtr[strlen(inputPtr)] = 0;

   return output;
}
