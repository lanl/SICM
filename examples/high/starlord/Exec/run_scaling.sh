
dir=scaling_results

mkdir -p $dir

ncell_list="128 192 256 384 512 768"
ngpu_list="1 2 4 6 12 18 24 36 48 72 96 120 144 168 192 216"

inputs=inputs.128
probin=probin

Castro_ex=Castro3d.pgi.MPI.CUDA.ex

if [ ! -e $dir/$Castro_ex ]; then
    cp $Castro_ex $dir/
    cp helm_table.dat $dir/
    cp probin $dir/
fi

job_types="amr uniform"
job_types="uniform"

for job in $job_types
do

    for ngpu in $ngpu_list
    do

        if [ $ngpu -gt 6 ]; then
            nnodes=$(echo "$ngpu / 6" | bc)
        else
            nnodes=1
        fi

        for ncell in $ncell_list
        do

            ncell_min=0
            ncell_max=0

            if [ $ngpu -eq 1 ]; then
                ncell_min=128
                ncell_max=128
            elif [ $ngpu -eq 2 ]; then
                ncell_min=128
                ncell_max=192
            elif [ $ngpu -eq 4 ]; then
                ncell_min=128
                ncell_max=256
            elif [ $ngpu -eq 6 ]; then
                ncell_min=192
                ncell_max=256
            elif [ $ngpu -eq 12 ]; then
                ncell_min=256
                ncell_max=384
            elif [ $ngpu -eq 18 ]; then
                ncell_min=256
                ncell_max=384
            elif [ $ngpu -eq 24 ]; then
                ncell_min=256
                ncell_max=512
            elif [ $ngpu -eq 36 ]; then
                ncell_min=384
                ncell_max=512
            elif [ $ngpu -eq 48 ]; then
                ncell_min=384
                ncell_max=512
            elif [ $ngpu -eq 72 ]; then
                ncell_min=512
                ncell_max=512
            elif [ $ngpu -eq 96 ]; then
                ncell_min=512
                ncell_max=768
            elif [ $ngpu -eq 120 ]; then
                ncell_min=512
                ncell_max=768
            elif [ $ngpu -eq 144 ]; then
                ncell_min=768
                ncell_max=768
            elif [ $ngpu -eq 168 ]; then
                ncell_min=768
                ncell_max=768
            elif [ $ngpu -eq 192 ]; then
                ncell_min=768
                ncell_max=768
            elif [ $ngpu -eq 216 ]; then
                ncell_min=768
                ncell_max=768
            fi

            if [ $ncell -gt $ncell_max ] || [ $ncell -lt $ncell_min ]; then
                continue
            fi

            grid_size_list=""

            if [ $ncell -eq 128 ]; then
                grid_size_list="32 64 128"
                grid_size_list="64"
            elif [ $ncell -eq 192 ]; then
                grid_size_list="32 64 96"
            elif [ $ncell -eq 256 ]; then
                grid_size_list="32 64 128"
            elif [ $ncell -eq 384 ]; then
                grid_size_list="64 96 128"
            elif [ $ncell -eq 512 ]; then
                grid_size_list="64 96 128"
            elif [ $ncell -eq 768 ]; then
                grid_size_list="64 96 128"
            elif [ $ncell -eq 1024 ]; then
                grid_size_list="64 96 128"
            fi

            for grid_size in $grid_size_list
            do

                suffix=ncell.$ncell.ngpu.$ngpu.grid.$grid_size
                if [ "$job" == "amr" ]; then
                    suffix=amr.$suffix                    
                fi
                new_inputs=inputs.$suffix
                new_run_script=run_script_$suffix.sh

                #          if [ -e $dir/StarLord.$suffix.out ]; then
                if [ -e $dir/$new_inputs ]; then
                    continue
                fi

                if [ $ngpu -lt 6 ]; then
                    n_rs_per_node=$ngpu
                else
                    n_rs_per_node=6
                fi

                cp $inputs $dir/$new_inputs
                sed -i "s/amr.n_cell.*/amr.n_cell = $ncell $ncell $ncell/g" $dir/$new_inputs
                sed -i "s/amr.max_grid_size.*/amr.max_grid_size = $grid_size/g" $dir/$new_inputs

                cp run_script.sh $dir/$new_run_script
                sed -i "s/#BSUB -o.*/#BSUB -o StarLord.$suffix.out/g" $dir/$new_run_script
                sed -i "s/#BSUB -e.*/#BSUB -e StarLord.$suffix.out/g" $dir/$new_run_script
                sed -i "s/#BSUB -nnodes.*/#BSUB -nnodes $nnodes/g" $dir/$new_run_script
                sed -i "0,/Castro_ex.*/s//Castro_ex=$Castro_ex/" $dir/$new_run_script
                sed -i "0,/starlord_inputs.*/s//starlord_inputs=$new_inputs/" $dir/$new_run_script
                sed -i "0,/n_mpi.*/s//n_mpi=$ngpu/" $dir/$new_run_script
                sed -i "0,/n_gpu.*/s//n_gpu=1/" $dir/$new_run_script
                sed -i "0,/n_rs_per_node.*/s//n_rs_per_node=$n_rs_per_node/" $dir/$new_run_script

                if [ "$job" == "amr" ]; then
                    sed -i "s/.*max\_level.*/amr\.max\_level=1/" $dir/$new_inputs
                fi

                cd $dir
                echo "Submitting job with suffix "$suffix
                bsub $new_run_script
                cd -

            done

        done
    done
done



# Now do the pure CPU scaling for comparison.

Castro_ex=Castro3d.pgi.MPI.OMP.ex

if [ ! -e $dir/$Castro_ex ]; then
    cp $Castro_ex $dir/
    cp helm_table.dat $dir/
    cp probin $dir/
fi

n_mpi_per_node=6
n_omp=7
n_cores=7

nnodes_list="1 2 4 8 16 20 24 28 32 36"
ncell_list="128 192 256 384"

for nnodes in $nnodes_list
do

  n_mpi=$(echo "$nnodes * $n_mpi_per_node" | bc)

  for ncell in $ncell_list
  do

      ncell_min=0
      ncell_max=0

      if [ $nnodes -eq 1 ]; then
          ncell_min=128
          ncell_max=128
      elif [ $nnodes -eq 2 ]; then
          ncell_min=128
          ncell_max=128
      elif [ $nnodes -eq 4 ]; then
          ncell_min=128
          ncell_max=192
      elif [ $nnodes -eq 8 ]; then
          ncell_min=192
          ncell_max=256
      elif [ $nnodes -eq 16 ]; then
          ncell_min=256
          ncell_max=384
      elif [ $nnodes -eq 20 ]; then
          ncell_min=256
          ncell_max=384
      elif [ $nnodes -eq 24 ]; then
          ncell_min=384
          ncell_max=384
      elif [ $nnodes -eq 28 ]; then
          ncell_min=384
          ncell_max=384
      elif [ $nnodes -eq 32 ]; then
          ncell_min=384
          ncell_max=384
      elif [ $nnodes -eq 36 ]; then
          ncell_min=384
          ncell_max=384
      fi

      if [ $ncell -gt $ncell_max ] || [ $ncell -lt $ncell_min ]; then
          continue
      fi

      grid_size_list=""

      if [ $ncell -eq 128 ]; then
          grid_size_list="16 32 64"
      elif [ $ncell -eq 192 ]; then
          grid_size_list="32 64"
      elif [ $ncell -eq 256 ]; then
          grid_size_list="32 64"
      elif [ $ncell -eq 384 ]; then
          grid_size_list="32 64"
      elif [ $ncell -eq 512 ]; then
          grid_size_list="32 64 96"
      elif [ $ncell -eq 768 ]; then
          grid_size_list="64 96 128"
      elif [ $ncell -eq 1024 ]; then
          grid_size_list="96 128 192"
      fi

      for grid_size in $grid_size_list
      do

          suffix=ncell.$ncell.ncpu.$nnodes.nmpi.$n_mpi.nomp.$n_omp.grid.$grid_size
          new_inputs=inputs.$suffix
          new_run_script=run_script_$suffix.sh

#          if [ -e $dir/StarLord.$suffix.out ]; then
          if [ -e $dir/$new_inputs ]; then
              continue
          fi

          if [ $n_mpi -lt 6 ]; then
              n_rs_per_node=$n_mpi
          else
              n_rs_per_node=6
          fi

          cp $inputs $dir/$new_inputs
          sed -i "s/amr.n_cell.*/amr.n_cell = $ncell $ncell $ncell/g" $dir/$new_inputs
          sed -i "s/amr.max_grid_size.*/amr.max_grid_size = $grid_size/g" $dir/$new_inputs

          cp run_script.sh $dir/$new_run_script
          sed -i "s/#BSUB -o.*/#BSUB -o StarLord.$suffix.out/g" $dir/$new_run_script
          sed -i "s/#BSUB -e.*/#BSUB -e StarLord.$suffix.out/g" $dir/$new_run_script
          sed -i "s/#BSUB -nnodes.*/#BSUB -nnodes $nnodes/g" $dir/$new_run_script
          sed -i "0,/Castro_ex.*/s//Castro_ex=$Castro_ex/" $dir/$new_run_script
          sed -i "0,/starlord_inputs.*/s//starlord_inputs=$new_inputs/" $dir/$new_run_script
          sed -i "0,/n_mpi.*/s//n_mpi=$n_mpi/" $dir/$new_run_script
          sed -i "0,/n_omp.*/s//n_omp=$n_omp/" $dir/$new_run_script
          sed -i "0,/n_cores.*/s//n_cores=$n_cores/" $dir/$new_run_script
          sed -i "0,/n_gpu.*/s//n_gpu=0/" $dir/$new_run_script
          sed -i "0,/n_rs_per_node.*/s//n_rs_per_node=$n_rs_per_node/" $dir/$new_run_script

          cd $dir
          echo "Submitting job with suffix "$suffix
          bsub $new_run_script
          cd -

      done

  done
done
