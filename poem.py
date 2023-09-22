# We couldn't figure out how to compile the .cpp program so we just did this instead
import random

def distribute_poem_lines(input_file, output_file, data_file, test_files):
    with open(input_file, 'r') as f:
        lines = [(i, line) for i, line in enumerate(f.readlines())]

    random.shuffle(lines)

    lines_per_file = len(lines) // len(test_files)

    for i, test_file in enumerate(test_files):
        start_index = i * lines_per_file
        end_index = start_index + lines_per_file if i != len(test_files) - 1 else len(lines)

        with open(test_file, 'w') as f:
            for line_number, line in lines[start_index:end_index]:
                f.write(f"{line_number} {line}")

    with open(data_file, 'w') as f:
        f.write(f"{output_file}\n")
        for file in test_files:
            f.write(f"{file}\n")

if __name__ == "__main__":
    input_file = "poem.txt"
    output_file = "test_file"
    data_file = "test_spec"
    test_files = []
    for i in range(10):
        test_files.append("test_file_" + str(i))

    distribute_poem_lines(input_file, output_file, data_file, test_files)