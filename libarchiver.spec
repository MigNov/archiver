Name:		libarchiver
Version:	0.0.1
Release:	1%{?dist}%{?extra_release}
Summary:	Archiver utility
Source:		http://www.migsoft.net/projects/archiver/archiver-%{version}.tar.gz
Group:		Libraries/Compression
License:	LGPLv2+
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root
BuildRequires:	libmincrypt
Requires:	libmincrypt

%description
Library for data archiving

%prep
%setup -q -n libarchiver-%{version}

%build
%configure
make %{?_smp_mflags}

%install
mkdir -p %{buildroot}/%{_libdir}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc LICENSE README
%{_bindir}/archiver
%{_libdir}/libarchiver.a
%{_libdir}/libarchiver.la
%{_libdir}/libarchiver.so
%{_libdir}/libarchiver.so.0
%{_libdir}/libarchiver.so.0.0.0
%{_includedir}/archiver.h

%changelog
* Sun 15 Jan 2012 Michal Novotny <mignov@gmail.com> - 0.0.1:
- First version of archiver library made public
