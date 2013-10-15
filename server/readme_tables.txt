This document describes the tables in the ECMNet database and how they are used as well
as some descriptive information regarding the columns in them.  If you choose to write
any SQL against the database, the information here will be extremely useful to you.


First a list of the tables:
   MasterComposite      - This retains the original composites added to the server for
                          factorization.

	MasterCompositeSlaveSync - This indicates which slaves need updates for a given
	                           MasterComposite.

   MasterCompositeWorkDone - A child table of MasterComposite that summarizes all factoring
                             work done for the master composite

   Composite            - A child table of MasterComposite that is a combination of master
                          composites without factors and composite co-factors that have been
                          added back via the recurse option.

   CompositeFactorEvent - A child table of Composite that holds detail associated with
                          each factoring event such as factor method and B1

   CompositeFactorEventDetail - A child table of CompositeFactorEventDetail that holds details
                                such as the factor value, its decimal length and whether or not
                                it is PRP.

   UserStats            - Stats by user, such as number of factors found, total work done, etc.


Column Notes:
   TotalWorkDone:
      This is a computed value that indicates the total amount of ECM work done computed as follows:
         for each B1
             work done = work done + (B1 * local curves done for that B1)
      It is used to help the server compute how many curves are still needed for each B1 when it sends
      work to clients.

   Difficulty:
      This is a computed value that gives an indication as to the difficulty of factoring the candidate.
      It is computed by multiplying the total work done by the square of the decimal length of the
      candidate.  Note that ECMNet cannot compute the decimal length of expressions.

   LastSyncTime:
      This is the date of the last sync with the master.  The master will send data for this candidate
      to a slave if LastSyncTime from slave is before LastUpdateTime in the master.  The slave will
      send data for this candidate to the master if LastUpdateTime > LastSyncTime.

   MasterCompositeName:
      This is the name of the original composite that was added to the server.  The server will track
      all composites derived from this composite in the Composite table.

   CurvesSinceLastReport:
      This is the number of curves done by clients connected to this server that have not been
      reported to the master server.

   LocalCurves:
      This is the number of curves done according to the master server plus the number of curves
      done by clients connected to this server.

   SentToMaster:
      This factor event was reported to the master.  Once reported to the master, the master is
      responsible for sending the e-mail (or sending to its master).


