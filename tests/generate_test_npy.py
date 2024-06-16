import argparse
import json
from pathlib import Path
from typing import IO

import numpy as np


def write_header(file: IO[str], data: dict[str, str]) -> None:
    header_guard = 'INCLUDE_TEST_NPY_PATHS_H'
    file.write(f'#ifndef {header_guard}\n#define {header_guard}\n')
    file.write('namespace test_path {\n')
    for name, path in data.items():
        file.write(f'constexpr auto {name} = {json.dumps(str(path))};\n')
    file.write(f'}};\n#endif // {header_guard}\n')


def generate_normal_data() -> dict[str, np.ndarray]:
    flow = np.arange(10, 0, -1, dtype=np.float64)
    flow_time = np.arange(flow.size, dtype=np.float64)
    event = np.mod(np.arange(flow.size), 2) == 0

    audio = np.arange(100) / 2
    audio_time = np.arange(audio.size, dtype=np.float64)

    return {
        'flow': flow,
        'flow_time': flow_time,
        'event': event,
        'audio': audio,
        'audio_time': audio_time,
    }


def generate_no_samples() -> dict[str, np.ndarray]:
    return {
        'flow': np.float64([]),
        'flow_time': np.float64([]),
        'event': np.uint8([]),
        'audio': np.float64([]),
        'audio_time': np.float64([]),
    }


def generate_equal_len() -> dict[str, np.ndarray]:
    data = generate_normal_data()
    data['audio'] = data['audio'][:data['flow'].size]
    data['audio_time'] = data['audio_time'][:data['flow'].size]
    return data


def generate_audio_field_shorter() -> dict[str, np.ndarray]:
    data = generate_normal_data()
    data['audio'] = data['audio'][:data['flow'].size - 1]
    data['audio_time'] = data['audio_time'][:data['flow'].size - 1]
    return data


def generate_missing_field(field: str) -> dict[str, np.ndarray]:
    data = generate_normal_data()
    del data[field]
    return data


def generate_invalid_flow_type() -> dict[str, np.ndarray]:
    data = generate_normal_data()
    data['flow'] = data['flow'].astype(np.float32)
    return data


def generate_shortened_field(field: str) -> dict[str, np.ndarray]:
    data = generate_normal_data()
    array = data[field]
    data[field] = array[:array.size // 2]
    return data


def generate_2d_array() -> dict[str, np.ndarray]:
    data = generate_normal_data()
    data['flow'] = np.column_stack((data['flow'], data['flow']))
    return data


def generate_fortran_order() -> dict[str, np.ndarray]:
    data = generate_normal_data()
    data['event'] = np.asfortranarray((data['event'], data['event']))
    return data


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--header', type=str, required=True)
    parser.add_argument('output_directory', type=str, nargs=1)
    args = parser.parse_args()

    header_path = args.header
    output_directory = Path(args.output_directory[0])
    if not output_directory.is_dir():
        raise ValueError(f'not a directory: {output_directory}')

    missing_field_prefix = str(output_directory / 'missing_')
    data: dict[str, str] = {
        'missing_field_prefix': missing_field_prefix,
    }

    normal_data = generate_normal_data()
    for field in normal_data.keys():
        path = f'{missing_field_prefix}{field}.npz'
        np.savez(path, **generate_missing_field(field))

    len_mismatch_prefix = str(output_directory / 'len_mismatch_')
    data['len_mismatch_prefix'] = len_mismatch_prefix
    for field in normal_data.keys():
        path = f'{len_mismatch_prefix}{field}.npz'
        np.savez(path, **generate_shortened_field(field))

    def generate(name: str, array: dict[str, np.ndarray]):
        path = output_directory / f'{name}.npz'
        data[name] = str(path)
        np.savez(path, **array)

    generate('normal', normal_data)
    generate('audio_flow_equal_len', generate_equal_len())
    generate('audio_field_shorter', generate_audio_field_shorter())
    generate('no_samples', generate_no_samples())
    generate('invalid_flow_type', generate_invalid_flow_type())
    generate('two_dimensional_array', generate_2d_array())
    generate('fortran_order', generate_fortran_order())

    with open(header_path, 'w') as f:
        write_header(f, data)


if __name__ == '__main__':
    main()
