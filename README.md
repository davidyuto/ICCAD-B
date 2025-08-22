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

---

## Example: testcase3

```bash
./cadb_1075_final \
  -weight testcase3/testcase3_weight \
  -lef testcase3/snps25hopt.lef testcase3/snps25lopt.lef testcase3/snps25ropt.lef testcase3/snps25slopt.lef \
  -sdc testcase3/testcase3.sdc \
  -v testcase3/testcase3.v \
  -def testcase3/testcase3.def \
  -out testcase3/output