 
  abort ();
}


 

_Noreturn void
openat_restore_fail (int errnum)
{
#ifndef GNULIB_LIBPOSIX
  error (exit_failure, errnum,
         _("failed to return to initial working directory"));
#endif

   
  abort ();
}
