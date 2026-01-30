%bcond_with     debug
%bcond_with     debuginfo

Name:           zfs-kmod
Version:        2.2.2
Release:        1%{?dist}

Summary:        Kernel module(s)
Group:          System Environment/Kernel
License:        CDDL
URL:            https://github.com/openzfs/zfs
BuildRequires:  %kernel_module_package_buildreqs
Source0:        zfs-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

# Additional dependency information for the kmod sub-package must be specified
# by generating a preamble text file which kmodtool can append to the spec file.
%(/bin/echo -e "\
Requires:       zfs = %{version}\n\
Conflicts:      zfs-dkms)

# LDFLAGS are not sanitized by arch/*/Makefile for these architectures.
%ifarch ppc ppc64 ppc64le aarch64
%global __global_ldflags %{nil}
%endif

%description
This package contains the ZFS kernel modules.

%define kmod_name zfs

%kernel_module_package -n %{kmod_name} -p %{_sourcedir}/kmod-preamble

%define ksrc %{_usrsrc}/kernels/%{kverrel}
%define kobj %{ksrc}

%package -n kmod-%{kmod_name}-devel
Summary:        ZFS kernel module(s) devel common
Group:          System Environment/Kernel

%description -n  kmod-%{kmod_name}-devel
This package provides the header files and objects to build kernel modules.

%prep
if ! [ -d "%{ksrc}"  ]; then
        echo "Kernel build directory isn't set properly, cannot continue"
        exit 1
fi

%if %{with debug}
%define debug --enable-debug
%else
%define debug --disable-debug
%endif

%if %{with debuginfo}
%define debuginfo --enable-debuginfo
%else
%define debuginfo --disable-debuginfo
%endif

%setup -n %{kmod_name}-%{version}
%build
%configure \
        --with-config=kernel \
        --with-linux=%{ksrc} \
        --with-linux-obj=%{kobj} \
        %{debug} \
        %{debuginfo} \
        %{?kernel_cc} \
        %{?kernel_ld} \
        %{?kernel_llvm}
make %{?_smp_mflags}

%install
make install \
        DESTDIR=${RPM_BUILD_ROOT} \
        INSTALL_MOD_DIR=extra/%{kmod_name}
%{__rm} -f %{buildroot}/lib/modules/%{kverrel}/modules.*

# find-debuginfo.sh only considers executables
%{__chmod} u+x  %{buildroot}/lib/modules/%{kverrel}/extra/*/*

%clean
rm -rf $RPM_BUILD_ROOT

%files -n kmod-%{kmod_name}-devel
%{_usrsrc}/%{kmod_name}-%{version}
