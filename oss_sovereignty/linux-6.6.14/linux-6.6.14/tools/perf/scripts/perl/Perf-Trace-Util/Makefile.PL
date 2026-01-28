use 5.010000;
use ExtUtils::MakeMaker;
WriteMakefile(
    NAME              => 'Perf::Trace::Context',
    VERSION_FROM      => 'lib/Perf/Trace/Context.pm', 
    PREREQ_PM         => {}, 
    ($] >= 5.005 ?     
      (ABSTRACT_FROM  => 'lib/Perf/Trace/Context.pm', 
       AUTHOR         => 'Tom Zanussi <tzanussi@gmail.com>') : ()),
    LIBS              => [''], 
    DEFINE            => '-I ../..', 
    INC               => '-I.', 
    OBJECT            => 'Context.o', 
);
