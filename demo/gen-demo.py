import argparse
import dataclasses
import hashlib
import json
import os.path
import random
from io import BytesIO

import numpy as np


FLOW_SAMPLE_RATE = 100
AUDIO_SAMPLE_RATE = 1000


def random_md5_hexdigest() -> str:
    return ''.join(random.choices('abcdef0123456789', k=32))


@dataclasses.dataclass
@dataclasses.dataclass
class TimeRange:
    start: float
    end: float


@dataclasses.dataclass
class FileInfo:
    path: str
    checksum: str


@dataclasses.dataclass
class SwallowTask:
    subject: int
    test_type: str
    repeatnum: int
    swallownum: int
    recording_file: str
    csv_range_secs: TimeRange
    event_range_secs: TimeRange
    npz_file: FileInfo


def random_sinusoid(t, f0, amplitude, sigma):
    phase = random.uniform(-2 * np.pi, 2 * np.pi)
    return (
        amplitude * np.sin(2 * np.pi * f0 * t + phase)
        + np.random.randn(t.size) * sigma
    )


def random_task_data(
    start: float, end: float, event_start: float, event_end: float
) -> dict[str, np.ndarray]:
    duration = end - start
    flow_time = np.linspace(
        start, end, round(duration * FLOW_SAMPLE_RATE), dtype=np.float64
    )
    audio_time = np.linspace(
        start, end, round(duration * AUDIO_SAMPLE_RATE), dtype=np.float64
    )
    event = np.zeros(flow_time.size, dtype=bool)
    event[(flow_time >= event_start) & (flow_time <= event_end)] = True

    return {
        'flow': random_sinusoid(
            flow_time, 1 + np.random.randn(1), 6 + np.random.randn(1), 0.5
        ),
        'flow_time': flow_time,
        'event': event,
        'audio_time': audio_time,
        'audio': random_sinusoid(
            audio_time, 3 + np.random.randn(1), 2 + np.random.randn(1), .5
        ),
    }


def corrupt_task_data(data) -> dict[str, np.ndarray]:
    # Make flow time a 32-bit integer
    return data | {'flow_time': data['flow_time'].astype(np.uint32)}


def gen_random_task(
    subject: int,
    data_dir: str,
    action: str | None,
) -> SwallowTask:
    test_type = random.choice(('tidal', 'cued'))
    repeatnum = random.randrange(3) + 1
    swallownum = random.randrange(10 if test_type == 'tidal' else 4) + 1
    path = f'{subject:02}-{test_type}-r{repeatnum}-s{swallownum}.npz'

    start_time = random.uniform(0, 10)
    end_time = start_time + random.uniform(10, 15)

    event_start = start_time + random.uniform(1, 5)
    event_end = event_start + random.uniform(.2, 1.5)

    bytesio = BytesIO()
    data_fields = random_task_data(
        start_time, end_time, event_start, event_end
    )
    if action == 'corrupt':
        data_fields = corrupt_task_data(data_fields)

    np.savez(bytesio, **data_fields)
    data = bytesio.getvalue()
    checksum = hashlib.md5(data).hexdigest()
    task = SwallowTask(
        subject=subject,
        test_type=test_type,
        repeatnum=repeatnum,
        swallownum=swallownum,
        recording_file=random_md5_hexdigest(),
        csv_range_secs=TimeRange(start=start_time, end=end_time),
        event_range_secs=TimeRange(start=event_start, end=event_end),
        npz_file=FileInfo(path=path, checksum=checksum),
    )

    if action != 'nosave':
        with open(os.path.join(data_dir, path), 'wb') as f:
            f.write(data)

    return task


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tasks-json', required=True)
    parser.add_argument('--data-dir', required=True)
    args = parser.parse_args()

    # First five tasks are normal
    tasks = [
        gen_random_task(subject=s + 1, data_dir=args.data_dir, action=None)
        for s in range(5)
    ]

    # Next three tasks are missing files
    tasks.extend(
        gen_random_task(
            subject=s + 1,
            data_dir=args.data_dir,
            action='nosave'
        )
        for s in range(5, 8)
    )

    # Next two tasks are corrupted
    tasks.extend(
        gen_random_task(
            subject=s + 1,
            data_dir=args.data_dir,
            action='corrupt'
        )
        for s in range(8, 10)
    )

    tasks_dict = [dataclasses.asdict(t) for t in tasks]
    with open(args.tasks_json, 'w') as f:
        json.dump(tasks_dict, f, indent=2)
        f.write('\n')


if __name__ == '__main__':
    main()
