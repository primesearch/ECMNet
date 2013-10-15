alter table mastercomposite drop column lastsynctime;
alter table mastercomposite  add SendUpdatesToMaster int not null;
