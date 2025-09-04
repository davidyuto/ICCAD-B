# ICCAD-B

---

## Execution Format

```bash
./cadb_1075_final \
    -weight <weightFile> \
    -lib <libFile1> <libFile2> ... \
    -lef <lefFile1> <lefFile2> ... \
    -db <dbFile1> <dbFile2> ... \
    -tf <tfFile1> <tfFile2> ... \
    -sdc <sdcFile1> <sdcFile2> ... \
    -v <verilogFile1> <verilogFile2> ... \
    -def <defFile1> <defFile2> ... \
    -out <outputName>
```
## Example : testcase1

```bash
./cadb_1075_final \
  -weight testcase1/testcase1_weight \
  -lib testcase3/snps25hopt_base_ff0p88v25c.lib \
       testcase3/snps25lopt_base_ff0p88v25c.lib \
       testcase3/snps25ropt_base_ff0p88v25c.lib \
       testcase3/snps25slopt_base_ff0p88v25c.lib \
  -lef testcase3/snps25hopt.lef \
       testcase3/snps25lopt.lef \
       testcase3/snps25ropt.lef \
       testcase3/snps25slopt.lef \
  -sdc testcase1/testcase1.sdc \
  -v   testcase1/testcase1.v \
  -def testcase1/testcase1.def \
  -out cadb_1075 
```

## Example : testcase2

```bash
./cadb_1075_final \
  -weight testcase2/testcase2_weight \
  -lib testcase3/snps25hopt_base_ff0p88v25c.lib \
       testcase3/snps25lopt_base_ff0p88v25c.lib \
       testcase3/snps25ropt_base_ff0p88v25c.lib \
       testcase3/snps25slopt_base_ff0p88v25c.lib \
  -lef testcase3/snps25hopt.lef \
       testcase3/snps25lopt.lef \
       testcase3/snps25ropt.lef \
       testcase3/snps25slopt.lef \
  -sdc testcase2/testcase2.sdc \
  -v   testcase2/testcase2.v \
  -def testcase2/testcase2.def \
  -out cadb_1075  
```

## Example : testcase3

```bash
./cadb_1075_final \
  -weight testcase3/testcase3_weight \
  -lib testcase3/snps25hopt_base_ff0p88v25c.lib \
       testcase3/snps25lopt_base_ff0p88v25c.lib \
       testcase3/snps25ropt_base_ff0p88v25c.lib \
       testcase3/snps25slopt_base_ff0p88v25c.lib \
  -lef testcase3/snps25hopt.lef \
       testcase3/snps25lopt.lef \
       testcase3/snps25ropt.lef \
       testcase3/snps25slopt.lef \
  -sdc testcase3/testcase3.sdc \
  -v   testcase3/testcase3.v \
  -def testcase3/testcase3.def \
  -out cadb_1075 
```