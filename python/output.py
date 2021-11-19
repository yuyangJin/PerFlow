import re
import os
import math
from distutils.spawn import find_executable

from exceptions import PyCallGraphException
from color import Color


class Output(object):
    '''Base class for all outputters.'''

    def __init__(self, **kwargs):
        self.node_color_func = self.node_color
        self.edge_color_func = self.edge_color
        self.node_label_func = self.node_label
        self.edge_label_func = self.edge_label
        self.edge_penwidth_func = self.edge_penwidth

        # Update the defaults with anything from kwargs
        [setattr(self, k, v) for k, v in kwargs.items()]

    def set_config(self, config):
        '''
        This is a quick hack to move the config variables set in Config into
        the output module config variables.
        '''
        for k, v in config.__dict__.items():
            if hasattr(self, k) and \
                    callable(getattr(self, k)):
                continue
            setattr(self, k, v)

    def node_color(self, node, percent):
        #value = float(node.id * 2) / 3
        #value = 0.0
        color = 0.0
        # if node.type_name == "MPI":
        #     color = 0.3
        # elif node.type_name == "FUNCTION":
        #     color = 0.5
        # elif node.type_name == "LOOP":
        #     color = 0.8
        # if node.performance_percentage > 0.0:
        value = float(math.sqrt(math.sqrt(percent * 100) * 10) / 10) ** 2
        #     #value = (node.sampling_count / self.total_sample_count) ** 2
        #     #print(performance_percentage)
            
        #     #print (node.sampling_count/self.total_sample_count, value)
        #     value *= 2
        #     if value > 1:
        #         value = 0.9
        #     return Color.hsv(color, value, 0.9)
        # # elif node.comm_time[-1] > 0.0:
        # #     value = float(math.sqrt(math.sqrt(math.sqrt(node.comm_time[-1] / self.total_comm_time * 100) * 10) * 10) / 10)** 3
        # #     #print(node.comm_time, self.total_comm_time)
        # #     return Color.hsv(color, value, 0.9)
        # else:
        return Color.hsv(color, value, 0.9)

    def edge_color(self, color):
        #value = float(edge_src * 2) / 3
        if color == 0:
            saturation = 0.3
            value = 0.5
        else:
            saturation = 1
            value = 1
        
        return Color.hsv(color, saturation, value)

    def node_label(self, vertex, vertex_attrs):
        parts = []

        for attr in vertex_attrs:
            if attr in vertex.attributes().keys():
                parts += [
                    '{}: {}'.format(attr, vertex[attr])
                ]

        return r'\n'.join(parts)

    def edge_label(self, edge, edge_attrs):
        parts = []

        for attr in edge_attrs:
            if attr in edge.attributes().keys():
                parts += [
                    '{}: {}'.format(attr, edge[attr])
                ]

        return r'\n'.join(parts)
        # return '{0}'.format(label)

    def edge_penwidth(self, width):
        return width

    def sanity_check(self):
        '''Basic checks for certain libraries or external applications.  Raise
        or warn if there is a problem.
        '''
        pass

    @classmethod
    def add_arguments(cls, subparsers):
        pass

    def reset(self):
        pass

    def set_processor(self, processor):
        self.processor = processor

    def start(self):
        '''Initialise variables after initial configuration.'''
        pass

    def update(self):
        '''Called periodically during a trace, but only when should_update is
        set to True.
        '''
        raise NotImplementedError('update')

    def should_update(self):
        '''Return True if the update method should be called periodically.'''
        return False

    def done(self):
        '''Called when the trace is complete and ready to be saved.'''
        raise NotImplementedError('done')

    def ensure_binary(self, cmd):
        if find_executable(cmd):
            return

        raise PyCallGraphException(
            'The command "{0}" is required to be in your path.'.format(cmd))

    def normalize_path(self, path):
        regex_user_expand = re.compile('\A~')
        if regex_user_expand.match(path):
            path = os.path.expanduser(path)
        else:
            path = os.path.expandvars(path)  # expand, just in case
        return path

    def prepare_output_file(self):
        if self.fp is None:
            self.output_file = self.normalize_path(self.output_file)
            self.fp = open(self.output_file, 'wb')

    def verbose(self, text):
        print(text)

    def debug(self, text):
        print(text)

    @classmethod
    def add_output_file(cls, subparser, defaults, help):
        subparser.add_argument(
            '-o', '--output-file', type=str, default=defaults.output_file,
            help=help,
        )
