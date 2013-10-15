#ifndef _HTMLGenerator_

#define _HTMLGenerator_

#include <string>
#include "defs.h"
#include "DBInterface.h"
#include "Socket.h"
#include "Log.h"
#include "server.h"

class HTMLGenerator
{
public:
   ~HTMLGenerator() {};

   void     SetDBInterface(DBInterface *dbInterface) { ip_DBInterface = dbInterface; };
   void     SetLog(Log *theLog) { ip_Log = theLog; };
   void     SetHTMLTitle(const char *htmlTitle) { is_HTMLTitle = htmlTitle; };
   void     SetProjectName(const char *projectName) { is_ProjectName = projectName; };
   void     SetB1Info(double minB1, b1_t *b1, int32_t b1Count) { id_MinB1 = minB1; ii_B1Count = b1Count; ip_B1 = b1; };

   void     Send(Socket *theSocket, const char *thePage);
protected:
   string      is_HTMLTitle;
   string      is_ProjectName;
   DBInterface *ip_DBInterface;
   Log        *ip_Log;
   double      id_MinB1;
   int32_t     ii_B1Count;
   b1_t       *ip_B1;

   void        CompositeInfo(Socket *theSocket, bool showDetails);
   void        MasterSummary(Socket *theSocket, bool mastersWithFactors);
   void        MasterFactorDetail(Socket *theSocket, const char *masterCompositeName);
   void        UserStats(Socket *theSocket);
   void        PendingWork(Socket *theSocket);

   void        CompositeWorkDone(Socket *theSocket, const char *masterCompositeName);
   double      DetermineB1(double totalWorkDone);
   char       *SplitAcrossLines(char *input);
};

#endif // #ifndef _HTMLGenerator_
