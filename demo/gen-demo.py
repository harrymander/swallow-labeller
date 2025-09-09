import argparse
from collections.abc import Iterable
import dataclasses
import datetime
import hashlib
import json
import os.path
import random
import textwrap
from io import BytesIO
from typing import Optional

import numpy as np


FLOW_SAMPLE_RATE = 1000
AUDIO_SAMPLE_RATE = 10_000


def random_md5_hexdigest() -> str:
    return ''.join(random.choices('abcdef0123456789', k=32))


@dataclasses.dataclass
class TimeRange:
    start: float
    end: float

    def __post_init__(self):
        if self.end <= self.start:
            raise ValueError("end must be greater than start")
        if self.start < 0:
            raise ValueError("start must be >= 0")


@dataclasses.dataclass
class FileInfo:
    path: str
    checksum: str


@dataclasses.dataclass
class SwallowTask:
    subject: int
    test_type: str
    repeatnum: int
    recording_file: str
    npz_file: FileInfo
    event_times: list[TimeRange]

    def annotation_id(self) -> str:
        return self.npz_file.path


@dataclasses.dataclass
class SwallowApneaAnnotation:
    is_ambiguous: bool
    pattern: str
    time: TimeRange


@dataclasses.dataclass
class AnnotationResult:
    swallow_apneas: list[SwallowApneaAnnotation]
    ear_clicks: list[TimeRange]
    non_respiratory_flow_events: list[TimeRange]
    notes: list[str]


@dataclasses.dataclass
class Annotation:
    created_time: str
    result: AnnotationResult
    last_modified_time: None | str


def random_sinusoid(t, f0, amplitude, sigma):
    phase = random.uniform(-2 * np.pi, 2 * np.pi)
    return (
        amplitude * np.sin(2 * np.pi * f0 * t + phase)
        + np.random.randn(t.size) * sigma
    )


def random_task_data(
    start: float,
    end: float,
    event_times: Iterable[TimeRange],
) -> dict[str, np.ndarray]:
    duration = end - start
    flow_time = np.linspace(
        start, end, round(duration * FLOW_SAMPLE_RATE), dtype=np.float64
    )
    audio_time = np.linspace(
        start, end, round(duration * AUDIO_SAMPLE_RATE), dtype=np.float64
    )

    event = np.zeros(flow_time.size, dtype=bool)
    for event_time in event_times:
        mask = (flow_time >= event_time.start) & (flow_time <= event_time.end)
        event[mask] = True

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
    action: Optional[str],
) -> SwallowTask:
    test_type = random.choice(('tidal', 'cued'))
    repeatnum = random.randrange(3) + 1
    path = f'{subject:02}-{test_type}-r{repeatnum}.npz'

    start_time = 0
    duration = 600
    end_time = start_time + duration + random.uniform(-30, 30)
    num_events = random.randint(2, 5)
    event_midtimes = (
        np.linspace(start_time, end_time, num_events + 2)[1:-1]
        + np.random.uniform(-10, 10, size=num_events)
    )
    event_times: list[TimeRange] = []
    for mid_time in event_midtimes:
        time_range = TimeRange(
            start=mid_time - random.uniform(0.5, 1.5),
            end=mid_time + random.uniform(0.5, 1.5),
        )
        event_times.append(time_range)

    bytesio = BytesIO()
    data_fields = random_task_data(start_time, end_time, event_times)
    if action == 'corrupt':
        data_fields = corrupt_task_data(data_fields)

    np.savez(bytesio, **data_fields)
    data = bytesio.getvalue()
    checksum = hashlib.md5(data).hexdigest()
    task = SwallowTask(
        subject=subject,
        test_type=test_type,
        repeatnum=repeatnum,
        recording_file=random_md5_hexdigest(),
        npz_file=FileInfo(path=path, checksum=checksum),
        event_times=event_times,
    )

    if action != 'nosave':
        with open(os.path.join(data_dir, path), 'wb') as f:
            f.write(data)

    return task


def random_text(k: int) -> str:
    ipsum = """Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do
    eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad
    minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex
    ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate
    velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat
    cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id
    est laborum"""
    return ' '.join(random.choices(ipsum.split(), k=k))


def gen_random_annotation(task: SwallowTask) -> Annotation:
    ear_clicks: list[TimeRange] = []
    swallow_apneas: list[SwallowApneaAnnotation] = []
    non_respiratory_flow_events: list[TimeRange] = []
    for event in task.event_times:
        apnea_start = event.start + random.uniform(-3, 3)
        apnea_end = apnea_start + random.uniform(0.5, 2)
        swallow_apneas.append(SwallowApneaAnnotation(
            is_ambiguous=random.uniform(0, 1) < 0.2,
            pattern=random.choice(("ex-ex", "ex-in", "in-in", "in-ex")),
            time=TimeRange(apnea_start, apnea_end),
        ))
        if random.uniform(0, 1) < 0.7:
            # SNRF before apnea
            t1 = apnea_start + random.uniform(-0.01, 0.01)
            t0 = t1 - random.uniform(0.05, 0.2)
            non_respiratory_flow_events.append(TimeRange(t0, t1))
        if random.uniform(0, 1) < 0.6:
            # SNRF after apnea
            t0 = apnea_end + random.uniform(-0.01, 0.01)
            t1 = t0 + random.uniform(0.05, 0.2)
            non_respiratory_flow_events.append(TimeRange(t0, t1))
        if random.uniform(0, 1) < 0.9:
            # Ear click
            t0 = event.start + random.uniform(-0.2, 0.2)
            t1 = t0 + random.uniform(0.01, 0.1)
            ear_clicks.append(TimeRange(t0, t1))

    notes = [
        random_text(random.randint(3, 6))
        for _ in range(random.randint(0, 3))
    ]
    result = AnnotationResult(
        notes=notes,
        ear_clicks=ear_clicks,
        swallow_apneas=swallow_apneas,
        non_respiratory_flow_events=non_respiratory_flow_events,
    )

    now = datetime.datetime.now(datetime.timezone.utc)
    last_modified = None
    if random.randint(0, 3):
        delta = datetime.timedelta(
            seconds=random.randint(1, 59),
            minutes=random.randint(1, 59),
            hours=random.randint(1, 12),
            days=random.randint(0, 10),
        )
        last_modified = now + delta

    time_fmt = '%Y-%m-%dT%H:%M:%SZ'
    return Annotation(
        result=result,
        created_time=now.strftime(time_fmt),
        last_modified_time=(
            last_modified.strftime(time_fmt)
            if last_modified else None
        ),
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tasks-json', required=True)
    parser.add_argument('--data-dir', required=True)
    parser.add_argument("--existing-annotations")
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

    tasks_json = [dataclasses.asdict(t) for t in tasks]
    with open(args.tasks_json, 'w') as f:
        json.dump(tasks_json, f, indent=2)
        f.write('\n')

    existing_annotations_path = args.existing_annotations
    if existing_annotations_path is not None:
        # Generate annotations for every second task
        existing_annotations = {
            task.npz_file.path: dataclasses.asdict(gen_random_annotation(task))
            for task in tasks[1::2]
        }
        with open(existing_annotations_path, 'w') as f:
            json.dump(existing_annotations, f, indent=2)
            f.write('\n')


if __name__ == '__main__':
    main()
