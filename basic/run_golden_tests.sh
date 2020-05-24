#! /bin/sh
set -e
formatter="$1"
in_dir="$2"
shift; shift

rv=0
dialects=$(cd "${in_dir}"/inputs && ls -1)
for dialect in ${dialects}
do
    input_files=$(cd ${in_dir}/inputs/${dialect} && ls -1)
    echo "Test input files for dialect ${dialect}: ${input_files}"

    for infile in ${input_files}
    do
	infile_path="${in_dir}/inputs/${dialect}/${infile}"
	if ! [ -f "${infile_path}" ]
	then
	    echo "Internal error: cannot find input file ${infile_path}" >&2
	    exit 2
	fi
	found_golden=false
	goldens_missing=""
	for how in 0 STDIN_0 1 2 3 4 5 6 7 STDIN_7
	do
	    listo="${how}"
	    use_stdin=false
	    case ${how} in
		STDIN_*)
		    listo="${how##STDIN_}"
		    use_stdin=true
		    ;;
		*)
		    listo="${how}"
		    ;;
	    esac

	    g_txt="${in_dir}/golden/${dialect}/${infile}_listo${listo}.txt"
	    g_bin="${in_dir}/golden/${dialect}/${infile}_listo${listo}.bin"
	    if [ -e "${g_txt}" ]
	    then
	        found_golden=true
		differ="diff"
		diff_opts="-u"
		g="${g_txt}"
	    elif [ -e "${g_bin}" ]
	    then
		 found_golden=true
		 differ=cmp
		 diff_opts=""
		 g="${g_bin}"
	    else
		goldens_missing="${goldens_missing} ${g_txt} ${g_bin}"
		continue
	    fi

	    out="$(mktemp)"

	    run_formatter() {
		if $use_stdin
		then
		    "${formatter}" --listo="${listo}" --dialect="${dialect}" - < "${infile_path}"
		else
		    "${formatter}" --listo="${listo}" --dialect="${dialect}" "${infile_path}"
		fi >| "${out}"
	    }

	    if run_formatter
	    then
	    	if "${differ}" ${diff_opts} "${g}" "${out}"
	    	then
	    		echo "PASS: ${g}"
	    	else
		        if [ "${differ}" = "cmp" ]; then hexdump -C "${out}" 2>&1; fi
	    		echo "FAIL: ${g}: wrong output" >&2
	    		rv=1
	    	fi
	     else
	    		echo "FAIL: ${g}: exit value was $?" >&2
	    		rv=1
	     fi
	    rm -f "${out}"
	done
	if ! ${found_golden}
	then
		echo "No golden file was found for input file ${infile}; looked in ${goldens_missing}" >&2
		rv=1
	fi
    done
done
exit "${rv}"
