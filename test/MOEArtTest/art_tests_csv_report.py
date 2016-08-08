import argparse
import os, shutil
import csv

def process_path(path):
    if len(path) > 1:
        if path[-1:] == os.sep:
            return path
        else:
            return path + os.sep
    else:
        return ''

def get_file_name(csv_dir):
    for d, dirs, files in os.walk(csv_dir):
        for f in files:
            if f.endswith('.csv'):
                return os.path.join(d,f)

def generate_csv_report(csv_dir, result_dir, build_type_id, build_id):
    csv_file = get_file_name(csv_dir)
    if csv_file:
        tests = []
        with open(csv_file) as csv_file:
            csv_reader = csv.reader(csv_file, delimiter='\t')
            for row in csv_reader:
                tests.append(row)
        with open(result_dir + 'report.csv', 'w', newline='') as csv_file:
            csv_writer = csv.writer(csv_file, delimiter=';')
            for row in tests:
                if row != tests[0]:
                    row[1] = 'fail' if row[1] != 'OK' else 'success'
                    row.append('https://teamcity01-ir.devtools.intel.com/viewLog.html?tab=buildLog&buildTypeId=' + build_type_id + '&buildId=' + build_id)
                else:
                    row[0] = 'Test'
                    row[1] = 'Status'
                    row.append('Link')
                csv_writer.writerow(row)
    else:
        print('CSV-file doesn\'t exist!')

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--csv_dir', dest='csv_dir', action='store', required = True, help='Directory with csv-file')
    parser.add_argument('--result_dir', dest='result_dir', action='store', required = True, help='Report directory')
    parser.add_argument('--build_type_id', dest='build_type_id', action='store', required = True, help='Build Type Id for Link')
    parser.add_argument('--build_id', dest='build_id', action='store', required = True, help='Build Id for Link')
    args = parser.parse_args()
    csv_dir = process_path(args.csv_dir)
    result_dir = process_path(args.result_dir)
    generate_csv_report(csv_dir, result_dir, args.build_type_id, args.build_id)
    print('Done!')