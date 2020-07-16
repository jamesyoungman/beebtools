#! /bin/sh
#
#   Copyright 2020 James Youngman
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
#
set -e
formatter="$1"
in_dir="$2"
shift; shift

rv=0
dialects=$(cd "${in_dir}"/inputs && ls -1)
for dialect in ${dialects}
do
    input_files=$(cd ${in_dir}/inputs/${dialect} && ls -1 | tr '\n' ' ')
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
                    label="$(printf 'listo %s, stdin' ${listo})"
                    ;;
                *)
                    listo="${how}"
                    label="$(printf 'listo %s, file ' ${listo})"
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
                        echo "PASS: ${label} ${g}"
                else
                        if [ "${differ}" = "cmp" ]; then hexdump -C "${out}" 2>&1; fi
                        echo "FAIL: ${label} ${g}: wrong output" >&2
                        rv=1
                fi
             else
                        echo "FAIL: ${label} ${g}: exit value was $?" >&2
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
