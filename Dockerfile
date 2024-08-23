FROM registry.sh.nextgenwaterprediction.com/ngwpc/nwm-ngen/nwm_base_image/nwm_base_image:NGWPC-3223-int

# ensure local python is preferred over distribution python
ENV PATH="/usr/local/bin:$PATH"

# cannot remove LANG even though https://bugs.python.org/issue19846 is fixed
# last attempted removal of LANG broke many users:
# https://github.com/docker-library/python/pull/570
ENV LANG="C.UTF-8"

ENV PYTHON_VERSION="3.10.14"
ENV CURLLIB_VERSION="8.5.0"
ENV ZLIB_VERSION="1.3.1"
ENV HDF5_VERSION="1.12.3"
ENV NETCDF_C_VERSION="4.7.4"
ENV NETCDF_FORTRAN_VERSION="4.5.4"
ENV BOOST_VERSION="1.79.0"

# Fix OpenMPI support within container
ENV PSM3_HAL=loopback
ENV PSM3_DEVICES=self

ENV PATH=$PATH:/opt/intel/compilers_and_libraries/linux/mpi/intel64/bin:/opt/intel/bin:/opt/intel/impi/2019.9.304/intel64/bin:/ngen-app/ngen-python/bin
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/opt/intel/compilers_and_libraries/linux/mpi/intel64/lib:/opt/intel/lib:/opt/intel/impi/2019.9.304/intel64/lib:/opt/intel/compilers_and_libraries_2020.4.304/linux/compiler/lib/intel64_lin:/usr/local/lib:/ngen-app/opt/lib

ENV CC=icc
ENV CXX=icpc
ENV FC=ifort
ENV F77=ifort

RUN set -eux; \
        \
        curl --location --output libcurl.gz "https://curl.se/download/curl-${CURLLIB_VERSION}.tar.gz"; \
        mkdir --parents /usr/src/libcurl_${CURLLIB_VERSION}; \
        tar --extract --directory /usr/src/libcurl_${CURLLIB_VERSION} --strip-components=1 --file libcurl.gz; \
        rm libcurl.gz; \
        \
        cd /usr/src/libcurl_${CURLLIB_VERSION}; \
        ./configure --prefix=/usr/local/ --with-openssl \
        ; \
        nproc="$(nproc)"; \
        make -j "$nproc" \
        ; \
        make install ;
    #\
    #rm --recursive --force /usr/src/libcurl_${CURLLIB_VERSION}

RUN set -eux; \
        \
        curl --location --output zlib.gz "https://www.zlib.net/zlib-${ZLIB_VERSION}.tar.gz"; \
        mkdir --parents /usr/src/zlib_${ZLIB_VERSION}; \
        tar --extract --directory /usr/src/zlib_${ZLIB_VERSION} --strip-components=1 --file zlib.gz; \
        rm zlib.gz; \
        \
        cd /usr/src/zlib_${ZLIB_VERSION}; \
        ./configure --prefix=/usr/local/ \
        ; \
        nproc="$(nproc)"; \
        make -j "$nproc" \
        ; \
        make install ;
    #\
    #rm --recursive --force /usr/src/zlib_${ZLIB_VERSION}

RUN set -eux; \
        \
        curl --location --output libhdf5.tar.gz "https://github.com/HDFGroup/hdf5/archive/refs/tags/hdf5-${HDF5_VERSION//./_}.tar.gz"; \
        mkdir --parents /usr/src/hdf5_${HDF5_VERSION}; \
        tar --extract --directory /usr/src/hdf5_${HDF5_VERSION} --strip-components=1 --file libhdf5.tar.gz; \
        rm libhdf5.tar.gz; \
        \
        cd /usr/src/hdf5_${HDF5_VERSION}; \
        ./configure --prefix=/usr/local/ --enable-fortran --enable-cxx --with-zlib=/usr/local/zlib_${ZLIB_VERSION} \
        ; \
        nproc="$(nproc)"; \
        make check -j "$nproc" \
        ; \
    make install ;
    #\
    #rm --recursive --force /usr/src/hdf5_${HDF5_VERSION}

# FIXME temporarily cleanup old netcdf files
RUN rm -f /usr/local/lib/*netcdf*
RUN rm -f /usr/local/include/*netcdf*

RUN set -eux; \
        \
        curl --location --output netcdf-c.tar.gz "https://github.com/Unidata/netcdf-c/archive/refs/tags/v${NETCDF_C_VERSION%%[a-z]*}.tar.gz"; \
        mkdir --parents /usr/src/netcdf-c_${NETCDF_C_VERSION}; \
        tar --extract --directory /usr/src/netcdf-c_${NETCDF_C_VERSION} --strip-components=1 --file netcdf-c.tar.gz; \
        rm netcdf-c.tar.gz; \
        \
        cd /usr/src/netcdf-c_${NETCDF_C_VERSION}; \
        CFLAGS="-I/usr/local/include/" \
        CPPFLAGS="-I/usr/local/include/" \
        LDFLAGS="-L/usr/local/lib/" \ 
        ./configure --prefix=/usr/local/ --enable-netcdf-4 --disable-dap \
        ; \
        nproc="$(nproc)"; \
        make -j "$nproc" \
        ; \
        make install ;
    #\
    #rm --recursive --force /usr/src/netcdf-c_${NETCDF_C_VERSION}

ENV CPP=icc-E
RUN set -eux; \
        \
        curl --location --output netcdf-fortran.tar.gz "https://github.com/Unidata/netcdf-fortran/archive/refs/tags/v${NETCDF_FORTRAN_VERSION%%[a-z]*}.tar.gz"; \
        mkdir --parents /usr/src/netcdf-fortran_${NETCDF_FORTRAN_VERSION}; \
        tar --extract --directory /usr/src/netcdf-fortran_${NETCDF_FORTRAN_VERSION} --strip-components=1 --file netcdf-fortran.tar.gz; \
        rm netcdf-fortran.tar.gz; \
        \
        cd /usr/src/netcdf-fortran_${NETCDF_FORTRAN_VERSION}; \
        CFLAGS="-I/usr/local/include/" \
        CPPFLAGS="-I/usr/local/include/" \
        LDFLAGS="-L/usr/local/lib/ -lnetcdf" \ 
        ./configure --prefix=/usr/local/ \
        ; \
        nproc="$(nproc)"; \
        make -j "$nproc" \
        ; \
        make install ;
    #\
    #rm --recursive --force /usr/src/netcdf-fortran_${NETCDF_FORTRAN_VERSION}

RUN set -eux; \
	\
	curl --location --output boost.tar.bz2 "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_VERSION%%[a-z]*}/source/boost_${BOOST_VERSION//./_}.tar.bz2"; \
	mkdir --parents /opt/boost; \
	tar --extract --directory /opt/boost --strip-components=1 --file boost.tar.bz2; \
	rm boost.tar.bz2

COPY . /ngen-app/ngen/

ENV VIRTUAL_ENV=/ngen-app/ngen-python
RUN set -eux; \
	\
    python3.10 -m venv ${VIRTUAL_ENV}
ENV PATH=${VIRTUAL_ENV}/bin:${PATH}

WORKDIR /ngen-app/

RUN set -eux; \
	\
    pip3 install -r ngen/extern/test_bmi_py/requirements.txt; \
    pip3 install -r ngen/extern/t-route/requirements.txt ; \
# Lock numpy and netcdf4 versions so t-route doesn't break
    pip3 install "numpy==1.26.4" "netcdf4<=1.6.3" ; \
    pip3 cache purge

WORKDIR /ngen-app/ngen/
RUN set -eux; \
    cmake -B cmake_build -S . \
        -DNGEN_WITH_MPI=ON \
        -DNGEN_WITH_NETCDF=ON \
        -DNGEN_WITH_SQLITE=ON \
        -DNGEN_WITH_UDUNITS=ON \
        -DNGEN_WITH_BMI_FORTRAN=ON \
        -DNGEN_WITH_BMI_C=ON \
        -DNGEN_WITH_PYTHON=ON \
        -DNGEN_WITH_TESTS=ON \
        -DNGEN_WITH_ROUTING=ON \
        -DNGEN_QUIET=ON \
        -DNGEN_UPDATE_GIT_SUBMODULES=OFF \
        -DBOOST_ROOT=/opt/boost/; \
    nproc="$(nproc)"; \
    cmake --build cmake_build --target all --parallel ${nproc} ; \
    \
    cmake -B extern/LASAM/cmake_build -S extern/LASAM/ -DNGEN=ON ; \
    cmake --build extern/LASAM/cmake_build/ ; \
    \
    cmake -B extern/snow17/cmake_build -S extern/snow17/ ; \
    cmake --build extern/snow17/cmake_build/ ; \
    \
    cmake -B extern/sac-sma/cmake_build -S extern/sac-sma/ ; \
    cmake --build extern/sac-sma/cmake_build/ ; \
    \
    cmake -B extern/SoilMoistureProfiles/cmake_build -S extern/SoilMoistureProfiles/SoilMoistureProfiles/ -DNGEN=ON ; \
    cmake --build extern/SoilMoistureProfiles/cmake_build/ ; \
    \
    cmake -B extern/SoilFreezeThaw/cmake_build -S extern/SoilFreezeThaw/SoilFreezeThaw/ -DNGEN=ON ; \
    cmake --build extern/SoilFreezeThaw/cmake_build/

ENV NETCDFALTERNATIVE=/opt/intel/compilers_and_libraries_2020.4.304/linux/mpi/intel64/include

RUN set -eux; \
	\
    cd extern/t-route ; \
    LDFLAGS="-Wl,-L/usr/local/lib/,-rpath,/usr/local/lib/" FC=ifort ./compiler.sh no-e ; \
    \
    pip3 cache purge

RUN set -eux; \
    mkdir --parents /ngencerf/data/ngen-run-logs/ ; \
    mkdir --parents /ngen-app/bin/ ; \
    mv run-ngen.sh /ngen-app/bin/ ; \
    chmod +x /ngen-app/bin/run-ngen.sh


WORKDIR /
SHELL ["/bin/bash", "-c"]

ENTRYPOINT [ "/ngen-app/bin/run-ngen.sh" ] 
CMD [ "--info" ]

