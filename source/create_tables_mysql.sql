-- Read server\readme_tables.txt for comments and descriptions of these tables and columns
-- InnoDB must be used because ECMNet needs a database engine that supports transactions
-- latin1_bin is used because column values are case sensitive

drop table if exists PendingWork;
drop table if exists UserStats;
drop table if exists CompositeFactorEventDetail;
drop table if exists CompositeFactorEvent;
drop table if exists Composite;
drop table if exists MasterCompositeSlaveSync;
drop table if exists MasterCompositeWorkDone;
drop table if exists MasterComposite;

create table PendingWork (
   UserID                  varchar(200)   collate latin1_bin,
   MachineID               varchar(200)   collate latin1_bin,
   ClientID                varchar(200)   collate latin1_bin,
   EmailID                 varchar(200)   collate latin1_bin,
   CompositeName           varchar(100)   not null,
   FactorMethod            varchar(20)    not null,
   B1                      double         not null,
   Curves                  int            not null,
   DateAssigned            bigint
) ENGINE=InnoDB;

create table UserStats (
   UserID                  varchar(200)   collate latin1_bin,
   WorkSinceLastReport     double         not null,
   FactorsSinceLastReport  int            not null,
   TimeSinceLastReport     double         not null,
   TotalWorkDone           double         not null,
   TotalFactorsFound       int            not null,
   TotalTime               double         not null,
   primary key (UserID)
) ENGINE=InnoDB;

create table MasterCompositeSlaveSync (
   MasterCompositeName     varchar(100)   not null,
   SlaveID                 varchar(200)   not null,
   SendUpdatesToSlave      int            not null,
   primary key (MasterCompositeName, SlaveID)
) ;

create table MasterComposite (
   MasterCompositeName     varchar(100)   not null,
   MasterCompositeValue    varchar(50000) not null,
   IsActive                int            not null,
   FullyFactored           int            not null,
   FactorCount             int            not null,
   RecurseIndex            int            not null,
   DecimalLength           int            not null,
   TotalWorkDone           double         not null,
   LastUpdateTime          bigint         not null,
   SendUpdatesToMaster     int            not null,
   primary key (MasterCompositeName),
   index ix_workdone (TotalWorkDone)
) ENGINE=InnoDB;

create table MasterCompositeWorkDone (
   MasterCompositeName     varchar(100)   not null,
   FactorMethod            varchar(20)    not null,
   B1                      double         not null,
   CurvesSinceLastReport   int            not null,
   LocalCurves             int            not null,
   primary key (MasterCompositeName, FactorMethod, B1),
   foreign key (MasterCompositeName) references MasterComposite (MasterCompositeName) on delete restrict
) ENGINE=InnoDB;

create table Composite (
   CompositeName           varchar(100)   not null,
   CompositeValue          varchar(50000) not null,
   MasterCompositeName     varchar(100)   not null,
   FactorEventCount        int            not null,
   DecimalLength           int            not null,
   Difficulty              double         not null,
   LastSendTime            bigint         not null,
   primary key (CompositeName),
   index ix_difficulty (Difficulty),
   index ix_length (DecimalLength),
   foreign key (MasterCompositeName) references MasterComposite (MasterCompositeName) on delete restrict
) ENGINE=InnoDB;

create table CompositeFactorEvent (
   CompositeName           varchar(100)   not null,
   FactorEvent             int            not null, 
   ReportedDate            bigint         not null,
   EmailID                 varchar(200)   collate latin1_bin,
   UserID                  varchar(200)   collate latin1_bin,
   MachineID               varchar(200)   collate latin1_bin,
   ClientID                varchar(200)   collate latin1_bin,
   ServerID                varchar(200)   collate latin1_bin,
   SentToMaster            int            not null,
   SentEmail               int            not null,
   Duplicate               int            not null,
   FactorMethod            varchar(20)    not null,
   FactorStep              int            not null,
   B1                      double         not null,
   B1Time                  double         not null,
   B2                      double         not null,
   B2Time                  double         not null,
   Sigma                   bigint         not null,
   X0                      bigint         not null,
   primary key (CompositeName, FactorEvent),
   foreign key (CompositeName) references Composite (CompositeName) on delete restrict
) ENGINE=InnoDB;

create table CompositeFactorEventDetail (
   CompositeName           varchar(100)   not null,
   FactorEvent             int            not null, 
   FactorEventSequence     int            not null,
   FactorValue             varchar(50000) not null,
   DecimalLength           int            not null,
   PRPTestResult           int            not null,
   IsRecursed              int            not null,
   primary key (CompositeName, FactorEvent, FactorEventSequence),
   foreign key (CompositeName, FactorEvent) references CompositeFactorEvent (CompositeName, FactorEvent) on delete restrict
) ENGINE=InnoDB;
