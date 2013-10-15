*************************** Master and Slave ********************************************

Master/slave combinations are used to configure multiple ECMNet servers to work on factoring
the same list of composites.  Each master can have one or more slaves.  It is possible for
a master to be a slave of another master server, in which case it is still referred to as
a slave.  The "master" server is the one that is not a slave to any other servers.

In a master/slave setup, the master is responsible for sending any e-mails for found factors
and the master is also responsible for taking composite co-factors and recursing them back
into the database as composites to be factored further.

To configure a master/slave, you need to do the following:

Set the password= parameter in the ini file to the same value for all masters and slaves.  This
is required in order for them to synchronize.

If setting up a slave serer, then you need to configure upstreamserver= and upstreamfrequency=
in the ini file of that slave.

Synchronization is initiated by a slave.  A slave will connect to the master based upon
the setting of the upstreamfrequency= parameter.  The slave pseudo-code for synchronization
with the master is as follows:
   Start upstream sync

   For each MasterComposite where SendUpdatesToMaster = 1
      Send WorkDone where CurvesSinceLastReport > 0
      Send Composites
      Send CompositeFactors where SentToMaster = 0

   For each User in UserStats where WorkSinceLastReport > 0
      Send UserStats data

   Upstream sync done

   Start downstream sync

   For each MasterComposite from the master
   	Delete WorkDone, Composites, and CompositeFactors
      Insert WorkDone, Composites, and CompositeFactors from Master

   Get UserStats data

   Downstream sync done

The master pseudo-code for synchronization with the slave is as follows:
   Start upstream sync

	For each MasterComposite with no row in SlaveSync for the slave
		Insert a row indicating sync is required

   For each MasterComposite from the slave
   	Apply updates to WorkDone, Composites, and CompositeFactors
   	Update SlaveSync to indiate syn is required with all slaves

   Get UserStats data

   Upstream sync done

   Start downstream sync

   For each MasterComposite where SlaveSynce = 1
      Send WorkDone
      Send Composites
      Send CompositeFactors

   For each User in UserStats
      Send UserStats data

   Downstream sync done


Slave servers have a number of limitations, including:
  1)  A composite added to a slave will not be added to the master.
  2)  If a composite is factored through a slave and one of the co-factors is composite, the
      master will be responsible for the INSERT of the composite co-factor into the Composite
      table.
  3)  Only the master will send e-mail for found factors.

Additional notes:
  In master/slave configurations, ECMNet 3.x servers are not backward compatible with
  ECMNet 2.x servers.  The servers just won't sync with one another, other than that I
  don't expect either to crash, but I haven't tested any combinations.

