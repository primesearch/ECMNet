-- Read server\readme_tables.txt for comments and descriptions of these tables and columns
-- InnoDB must be used because ECMNet needs a database engine that supports transactions
-- latin1_bin is used because column values are case sensitive

drop table PendingWork cascade;
drop table UserStats cascade;
drop table CompositeFactorEventDetail cascade;
drop table CompositeFactorEvent cascade;
drop table Composite cascade;
drop table MasterCompositeSlaveSync cascade;
drop table MasterCompositeWorkDone cascade;
drop table MasterComposite cascade;

create table PendingWork (
   UserID                  varchar(200)      not null,
   MachineID               varchar(200)      not null,
   InstanceID              varchar(200)      not null,
   EmailID                 varchar(200)      not null,
   CompositeName           varchar(100)      not null,
   FactorMethod            varchar(20)       not null,
   B1                      double precision  not null,
   Curves                  int               not null,
   DateAssigned            bigint
) ;

create table UserStats (
   UserID                  varchar(200)      not null,
   WorkSinceLastReport     double precision  not null,
   FactorsSinceLastReport  int               not null,
   TimeSinceLastReport     double precision  not null,
   TotalWorkDone           double precision  not null,
   TotalFactorsFound       int               not null,
   TotalTime               double precision  not null
) ;

create unique index pk_userstats on UserStats (UserID);

create table MasterCompositeSlaveSync (
   MasterCompositeName     varchar(100)      not null,
   SlaveID                 varchar(200)      not null,
   SendUpdatesToSlave      int               not null
) ;

create unique index pk_MasterCompositeSlaveSync on MasterCompositeSlaveSync (MasterCompositeName, SlaveID);

create table MasterComposite (
   MasterCompositeName     varchar(100)      not null,
   MasterCompositeValue    varchar(50000)    not null,
   IsActive                int               not null,
   FullyFactored           int               not null,
   FactorCount             int               not null,
   RecurseIndex            int               not null,
   DecimalLength           int               not null,
   TotalWorkDone           double precision  not null,
   LastUpdateTime          bigint            not null,
   SendUpdatesToMaster     int               not null
) ;

create unique index pk_mastercomposite on MasterComposite (MasterCompositeName);
create index ix_workdone on MasterComposite (TotalWorkDone);

create table MasterCompositeWorkDone (
   MasterCompositeName     varchar(100)      not null,
   FactorMethod            varchar(20)       not null,
   B1                      double precision  not null,
   CurvesSinceLastReport   int               not null,
   LocalCurves             int               not null,
   foreign key (MasterCompositeName) references MasterComposite (MasterCompositeName) on delete restrict
) ;

create unique index pk_mastercompositeworkdone on MasterCompositeWorkDone (MasterCompositeName, FactorMethod, B1);

create table Composite (
   CompositeName           varchar(100)      not null,
   CompositeValue          varchar(50000)    not null,
   MasterCompositeName     varchar(100)      not null,
   FactorEventCount        int               not null,
   DecimalLength           int               not null,
   Difficulty              double precision  not null,
   LastSendTime            bigint            not null,
   foreign key (MasterCompositeName) references MasterComposite (MasterCompositeName) on delete restrict
) ;

create unique index pk_composite on Composite (CompositeName);
create index ix_difficulty on Composite (Difficulty);
create index ix_length on Composite (DecimalLength);

create table CompositeFactorEvent (
   CompositeName           varchar(100)      not null,
   FactorEvent             int               not null, 
   ReportedDate            bigint            not null,
   EmailID                 varchar(200)      not null,
   UserID                  varchar(200)      not null,
   MachineID               varchar(200)      not null,
   InstanceID              varchar(200)      not null,
   ServerID                varchar(200)      not null,
   SentToMaster            int               not null,
   SentEmail               int               not null,
   Duplicate               int               not null,
   FactorMethod            varchar(20)       not null,
   FactorStep              int               not null,
   B1                      double precision  not null,
   B1Time                  double precision  not null,
   B2                      double precision  not null,
   B2Time                  double precision  not null,
   Sigma                   bigint            not null,
   X0                      bigint            not null,
   foreign key (CompositeName) references Composite (CompositeName) on delete restrict
) ;

create unique index pk_compositefactorevent on CompositeFactorEvent (CompositeName, FactorEvent);

create table CompositeFactorEventDetail (
   CompositeName           varchar(100)      not null,
   FactorEvent             int               not null, 
   FactorEventSequence     int               not null,
   FactorValue             varchar(50000)    not null,
   DecimalLength           int               not null,
   PRPTestResult           int               not null,
   IsRecursed              int               not null,
   foreign key (CompositeName, FactorEvent) references CompositeFactorEvent (CompositeName, FactorEvent) on delete restrict
) ;

create unique index pk_compositefactoreventdetail on CompositeFactorEventDetail (CompositeName, FactorEvent, FactorEventSequence);

